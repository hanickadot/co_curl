#ifndef CO_CURL_SCHEDULER_HPP
#define CO_CURL_SCHEDULER_HPP

#include "easy.hpp"
#include "multi.hpp"
#include "task_counter.hpp"
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <coroutine>

using CURL = void;

namespace co_curl {

struct coroutine_handle_queue {
	std::queue<std::coroutine_handle<>> data{};

	bool is_something_ready() const noexcept {
		return !data.empty();
	}

	void insert(std::coroutine_handle<> handle) {
		data.emplace(handle);
	}

	auto take_one() -> std::coroutine_handle<> {
		if (data.empty()) {
			return {};
		}

		const auto next = data.front();
		data.pop();
		return next;
	}
};

struct waiting_coroutines_for_curl_finished {
	// std::map<CURL *, std::coroutine_handle<>> data{};
	multi_handle curl{};
	result code{};

	waiting_coroutines_for_curl_finished() {
		curl.max_total_connections(8);
	}

	void insert(easy_handle & trigger, std::coroutine_handle<> coro_handle) {
		trigger.set_coroutine_handle(coro_handle);
		curl.add_handle(trigger);
	}

	auto trigger(CURL * trigger) noexcept -> std::coroutine_handle<>;

	auto complete_something(std::chrono::milliseconds timeout = std::chrono::milliseconds{100}) -> std::coroutine_handle<> {
		for (;;) {
			const auto r = curl.sync_perform();

			if (!r.has_value()) {
				assert(false);
				// TODO report problem
				break;
			}

			if (const auto f = curl.get_finished()) {
				this->code = {.code = f->code};
				return trigger(f->handle);
			}

			if (*r == 0) {
				break;
			}

			(void)curl.poll(timeout);
		}

		return {};
	}
};

template <typename Scheduler, typename Promise, typename T> concept scheduler_transforms = requires(Scheduler & sch, std::coroutine_handle<Promise> current, T obj) {
	{ sch.transform(current, obj) };
};

struct thread_and_finish_status {
	std::coroutine_handle<> coro;
	std::thread thread;
	bool is_finished;
	std::coroutine_handle<> awaiting_coro;
};

template <typename R, typename Scheduler> struct promise_type;

struct default_scheduler: task_counter {
	std::mutex mutex{};
	std::condition_variable cv{};
	coroutine_handle_queue ready{};
	waiting_coroutines_for_curl_finished waiting{};
	std::queue<thread_and_finish_status> join_queue{};
	std::multimap<std::coroutine_handle<>, std::coroutine_handle<>> waiting_for_someone_else{};
	std::map<std::coroutine_handle<>, std::thread> threads{};

	auto schedule_later(std::coroutine_handle<> h, co_curl::easy_handle & curl) -> std::coroutine_handle<> {
		{
			auto guard = std::lock_guard{mutex};
			waiting.insert(curl, h);
			task_counter::blocked();
		}

		return select_next_coroutine();
	}

	template <typename R, typename Scheduler> auto switch_thread(std::coroutine_handle<co_curl::promise_type<R, Scheduler>> h) -> std::coroutine_handle<> {
		{ // TODO: this will break my counting mechanism
			auto guard = std::lock_guard{mutex};
			// TODO block from using I/O outside main thread
			task_counter::switched_to_different_thread();
			auto it = threads.emplace(h, std::thread{});
			it.first->second = std::thread{[this, h] {
				h.resume();
				// in case we start other thread or something
				this->finish_thread(h, false);
			}};
		}

		return select_next_coroutine();
	}

	template <typename R, typename Scheduler> bool finish_thread(std::coroutine_handle<co_curl::promise_type<R, Scheduler>> h, bool can_continue = true) {
		auto guard = std::lock_guard{mutex};
		if (auto it = threads.find(h); it != threads.end()) {
			join_queue.emplace(h, std::move(it->second), !can_continue, h.promise().awaiter);
			threads.erase(it);

			cv.notify_all();
			return true;
		} else {
			return false;
		}
	}

	auto suspend() -> std::coroutine_handle<> {
		{
			auto guard = std::lock_guard{mutex};
			task_counter::blocked();
		}

		return select_next_coroutine();
	}

	auto select_next_coroutine(std::coroutine_handle<> immediate_awaiter = {}) -> std::coroutine_handle<> {
		auto guard = std::unique_lock{mutex};

		for (;;) {
			// join finished threads

			if (immediate_awaiter) {
				// std::cout << "[immediate awaiter]\n";
				task_counter::unblocked();
				return immediate_awaiter;
			}

			if (auto next_ready = ready.take_one()) {
				// std::cout << "[next_ready]\n";
				task_counter::unblocked();
				return next_ready;
			}

			if (task_counter::graph_blocked()) {
				// std::cout << "[blocked]\n";
				if (auto next_completed = waiting.complete_something()) {
					// std::cout << " [completed]\n";
					task_counter::unblocked();
					return next_completed;
				}

				// if there is nothing else to do, and we have some threads
				if (threads.size()) {
					// wait for some of them to finish
					cv.wait(guard);

					// and maybe they gave us something to resume
					continue;
				}
			}

			if (!join_queue.empty()) {
				auto && [coro, thread, is_finished, awaiter] = join_queue.front();
				guard.unlock();
				thread.join();
				guard.lock();
				task_counter::switched_back_to_main_thread();
				if (is_finished) {
					this->finish();
					wakeup_coroutines_waiting_for_unlocked(coro);
					immediate_awaiter = awaiter;

				} else {
					// we can continue in main thread
					ready.insert(coro);
				}

				join_queue.pop();
				continue; // try everything again
			}

			// task_counter::print();

			// std::cout << "------\n";
			return std::noop_coroutine();
		}
	}

	void wakeup_coroutines_waiting_for_unlocked(std::coroutine_handle<> awaited) {
		// auto guard = std::unique_lock{mutex};
		const auto [f, l] = waiting_for_someone_else.equal_range(awaited);

		for (auto it = f; it != l; it = waiting_for_someone_else.erase(it)) {
			ready.insert(it->second);
		}
	}

	void wakeup_coroutines_waiting_for(std::coroutine_handle<> awaited) {
		auto guard = std::unique_lock{mutex};
		wakeup_coroutines_waiting_for_unlocked(awaited);
	}

	void add_additional_awaiter(std::coroutine_handle<> awaited, std::coroutine_handle<> sleeping) {
		waiting_for_someone_else.emplace(awaited, sleeping);
	}

	void remove_additional_awaiter(std::coroutine_handle<> awaited, std::coroutine_handle<> sleeping) {
		const auto [f, l] = waiting_for_someone_else.equal_range(awaited);

		for (auto it = f; it != l;) {
			if (it->second == sleeping) {
				it = waiting_for_someone_else.erase(it);
			} else {
				++it;
			}
		}
	}

	auto get_curl() -> multi_handle & {
		return waiting.curl;
	}
};

template <typename T = default_scheduler> auto get_scheduler() -> T & {
	static T global_scheduler{};
	return global_scheduler;
}

} // namespace co_curl

#endif
