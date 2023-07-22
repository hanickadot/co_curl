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
		curl.remove_handle(trigger);

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

			// std::cout << "> remaining = " << *r << "\n";

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

struct task_counter {
	unsigned tasks_in_running{0};
	unsigned tasks_blocked{0};

	void start() noexcept {
		++tasks_in_running;
	}

	void finish() noexcept {
		--tasks_in_running;
	}

	void before_sleep() noexcept {
		++tasks_blocked;
	}

	void after_wakeup() noexcept {
		--tasks_blocked;
	}

	bool graph_blocked() const noexcept {
		return tasks_in_running == tasks_blocked;
	}

	void print() const noexcept {
		std::cout << "tasks: " << tasks_in_running << ", blocked: " << tasks_blocked;

		if (graph_blocked()) {
			std::cout << " [blocked]";
		}

		std::cout << "\n";
	}
};

struct default_scheduler: task_counter {
	coroutine_handle_queue ready{};
	waiting_coroutines_for_curl_finished waiting{};

	auto task_ready(std::coroutine_handle<> h) noexcept -> std::coroutine_handle<> {
		return next_coroutine(h);
	}

	auto schedule_later(std::coroutine_handle<> h, co_curl::easy_handle & curl) -> std::coroutine_handle<> {
		waiting.insert(curl, h);
		return next_coroutine();
	}

	auto next_coroutine(std::coroutine_handle<> immediate_awaiter = {}) -> std::coroutine_handle<> {
		// task_counter::print();

		if (immediate_awaiter) {
			// std::cout << "[immediate awaiter]\n";
			return immediate_awaiter;
			// TODO add queue
		} else if (auto next_ready = ready.take_one()) {
			// std::cout << "[next_ready]\n";
			return next_ready;

		} else if (task_counter::graph_blocked()) {
			// std::cout << "[blocked]\n";
			if (auto next_completed = waiting.complete_something()) {
				// std::cout << " [completed]\n";
				return next_completed;
			} else {
				// std::cout << "nothing to complete!\n";
			}
		}
		// std::cout << "------\n";
		return std::noop_coroutine();
	}

	// template <typename Promise> auto transform(Promise & promise, co_curl::perform perf) const {
	//	// transform blocking perform into lazy one
	//	return co_curl::perform_later{.easy = perf.handle, .multi = promise.scheduler.waiting.curl};
	// }
};

template <typename T> auto get_scheduler() -> T & {
	static T global_scheduler{};
	return global_scheduler;
}

} // namespace co_curl

#endif
