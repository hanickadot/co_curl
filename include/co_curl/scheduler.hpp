#ifndef CO_CURL_SCHEDULER_HPP
#define CO_CURL_SCHEDULER_HPP

#include "easy.hpp"
#include "task_counter.hpp"
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
	result code{};

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
				void * handle = msg->visit([&](multi_handle::finished f) {
					this->code = {.code = f.code};
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

struct default_scheduler: task_counter {
	coroutine_handle_queue ready{};
	waiting_coroutines_for_curl_finished waiting{};
	std::multimap<std::coroutine_handle<>, std::coroutine_handle<>> waiting_for_someone_else{};

	auto schedule_later(std::coroutine_handle<> h, co_curl::easy_handle & curl) -> std::coroutine_handle<> {
		waiting.insert(curl, h);
		task_counter::blocked();
		return select_next_coroutine();
	}

	auto suspend() -> std::coroutine_handle<> {
		task_counter::blocked();
		return select_next_coroutine();
	}

	auto select_next_coroutine(std::coroutine_handle<> immediate_awaiter = {}) -> std::coroutine_handle<> {
		if (immediate_awaiter) {
			// std::cout << "[immediate awaiter]\n";
			task_counter::unblocked();
			return immediate_awaiter;
		} else if (auto next_ready = ready.take_one()) {
			// std::cout << "[next_ready]\n";
			task_counter::unblocked();
			return next_ready;

		} else if (task_counter::graph_blocked()) {
			// std::cout << "[blocked]\n";
			if (auto next_completed = waiting.complete_something()) {
				// std::cout << " [completed]\n";
				task_counter::unblocked();
				return next_completed;
			}
		}
		// std::cout << "------\n";
		return std::noop_coroutine();
	}

	void wakeup_coroutines_waiting_for(std::coroutine_handle<> awaited) {
		const auto [f, l] = waiting_for_someone_else.equal_range(awaited);

		for (auto it = f; it != l; it = waiting_for_someone_else.erase(it)) {
			ready.insert(it->second);
		}
	}

	auto suspend_additional_awaiter(std::coroutine_handle<> awaited, std::coroutine_handle<> sleeping) {
		waiting_for_someone_else.emplace(awaited, sleeping);
		return suspend();
	}
};

template <typename T> auto get_scheduler() -> T & {
	static T global_scheduler{};
	return global_scheduler;
}

} // namespace co_curl

#endif
