#ifndef CO_CURL_SCHEDULER_HPP
#define CO_CURL_SCHEDULER_HPP

#include "easy.hpp"
#include <iostream>
#include <map>
#include <queue>
#include <cassert>
#include <chrono>
#include <coroutine>

using CURL = void;

namespace co_curl {

struct coroutine_handle_queue {
	std::queue<std::coroutine_handle<>> data{};

	void insert(std::coroutine_handle<> handle) {
		data.emplace(handle);
	}

	auto get_next() -> std::coroutine_handle<> {
		if (data.empty()) {
			return {};
		}

		const auto next = data.front();
		data.pop();
		return next;
	}
};

struct waiting_coroutines_for_curl_finished {
	std::map<CURL *, std::coroutine_handle<>> data{};
	multi_handle curl{};

	void insert(easy_handle & trigger, std::coroutine_handle<> coro_handle) {
		[[maybe_unused]] const auto r = data.emplace(trigger.native_handle, coro_handle);
		assert(r.second);
		curl.add_handle(trigger);
	}

	auto trigger(CURL * trigger) -> std::coroutine_handle<> {
		const auto it = data.find(trigger);

		assert(it != data.end());

		const auto handle = it->second;

		data.erase(it);

		return handle;
	}

	bool empty() const noexcept {
		return data.empty();
	}

	auto complete_something(std::chrono::milliseconds timeout = std::chrono::milliseconds{100}) -> std::coroutine_handle<> {
		while (!empty()) {
			const auto r = curl.sync_perform();

			if (!r.has_value()) {
				assert(false);
				// TODO report problem
				break;
			}

			if (const auto msg = curl.info_read()) {
				void * handle = msg->visit([](multi_handle::finished f) {
					return f.handle;
				});

				return trigger(handle);
			}

			if (*r == 0) {
				break;
			}

			(void)curl.poll(timeout);
		}

		return {};
	}
};

struct default_scheduler {
	// all resuming is going to happen thru a coroutine_handle return

	waiting_coroutines_for_curl_finished waiting{};
	coroutine_handle_queue ready{};

	unsigned started_tasks{0};
	unsigned finished_tasks{0};
	unsigned blocked_tasks{0};

	void start() noexcept {
		started_tasks++;
	}

	void finish() noexcept {
		finished_tasks++;
	}

	bool is_blocked() const noexcept {
		// std::cout << "non-blocked: " << (started_tasks - finished_tasks) << "; blocked:" << blocked_tasks << " ";

		const auto existing_tasks = (started_tasks - finished_tasks);
		const bool r = existing_tasks == blocked_tasks;

		// assert(existing_tasks >= blocked_tasks);

		// if (r) std::cout << " [BLOCKED]\n";
		// else
		//	std::cout << "\n";

		// assert(existing_tasks >= blocked_tasks);

		return r;
	}

	// this is called when someone is awaiting on a coroutine and it would block...
	auto do_something_else(std::coroutine_handle<> awaiter = {}) -> std::coroutine_handle<> {
		if (awaiter) {
			return awaiter;
		} else if (const auto next = ready.get_next()) {
			// if we have something ready, run it
			return schedule_now(next);
		} else if (!is_blocked()) {
			// leave current coroutine
			return std::noop_coroutine();
		} else if (const auto unblocked_next = waiting.complete_something()) {
			return schedule_now(unblocked_next);
		}
		// we are blocked or we can complete something
		std::terminate();
	}

	auto loop_to_finish() -> std::coroutine_handle<> {
		if (auto next = waiting.complete_something()) {
			blocked_tasks--;
			return next;
		}
		return {};
	}

	auto schedule_now(std::coroutine_handle<> h) noexcept -> std::coroutine_handle<> {
		--blocked_tasks;
		return h;
	}

	auto schedule_next() -> std::coroutine_handle<> {
		return do_something_else();
	}

	void set_additional_awaiters(std::coroutine_handle<> awaitee, std::coroutine_handle<> awaiter) {
		(void)awaitee;
		(void)awaiter;
		assert(false);
	}

	// our scheduler is interested in cu_curl::perform and will use multicurl instead
	template <typename Promise> auto await_differently_on(std::coroutine_handle<Promise> current, co_curl::perform perf) {
		// add to waiting coroutines and curl multi
		waiting.insert(perf.handle, current);
		++blocked_tasks;
		// TODO provide result code
		return co_curl::perform::lazy_perform();
	}

	// and we pass everything else thru without changes
	template <typename Promise, typename Awaitable> auto await_differently_on(std::coroutine_handle<Promise> current, Awaitable && object) {
		(void)current;
		++blocked_tasks;
		return std::forward<Awaitable>(object);
	}
};

template <typename T> auto get_scheduler() -> T & {
	static T global_scheduler{};
	return global_scheduler;
}

} // namespace co_curl

#endif
