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

template <typename Scheduler> struct suspend_and_jump_to_awaiter_or_use_scheduler {
	Scheduler & scheduler;
	std::coroutine_handle<> awaiter;

	constexpr bool await_ready() const noexcept { return false; }
	constexpr void await_resume() const noexcept { }

	auto await_suspend(std::coroutine_handle<>) noexcept {
		// short-circuit
		if (awaiter) {
			return awaiter;
		}

		return scheduler.select_next();
	}
};

template <typename Scheduler> suspend_and_jump_to_awaiter_or_use_scheduler(Scheduler &, std::coroutine_handle<>) -> suspend_and_jump_to_awaiter_or_use_scheduler<Scheduler>;

template <typename T> concept promise_has_awaiter = requires(T handle) {
	{ handle.promise().get_awaiter() };
};

template <typename T> bool awaiter_being_awaited_on(std::coroutine_handle<T> handle) {
	if constexpr (promise_has_awaiter<T>) {
		std::cout << "--- has_awaiter\n";
		return static_cast<bool>(handle.promise().get_awaiter());
	} else {
		return false;
	}
}

struct default_scheduler {
	// all resuming is going to happen thru a coroutine_handle return

	waiting_coroutines_for_curl_finished waiting{};
	coroutine_handle_queue ready{};
	std::coroutine_handle<> root{};

	// this is called when someone is awaiting on a coroutine and it would block...
	template <typename T> auto do_something_else(std::coroutine_handle<T> awaiter) -> std::coroutine_handle<> {
		if (const auto next = ready.get_next()) {
			// if we have something ready, run it
			return next;
		} else {
			// otherwise cut the chain, as we will restart from top again (so we can start as much asynchronous actions as possible before moving on)
			// trying to complete something would lead to blocking and downloading in smaller batches
			return std::noop_coroutine();
		}
	}

	auto loop_to_finish() -> std::coroutine_handle<> {
		return waiting.complete_something();
	}

	auto coroutine_finished(std::coroutine_handle<> awaiter) noexcept {
		return suspend_and_jump_to_awaiter_or_use_scheduler{*this, awaiter};
	}

	auto select_next() -> std::coroutine_handle<> {
		if (const auto first_in_queue = ready.get_next()) {
			// if there is no awaiter, look for coroutines which are ready...
			return first_in_queue;
		} else if (const auto first_completed = waiting.complete_something()) {
			// if there is something just
			return first_completed;
		} else {
			// and if nothing can be done, finished the chain
			return std::noop_coroutine();
		}
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

		// TODO provide result code
		return co_curl::perform::lazy_perform();
	}

	// and we pass everything else thru without changes
	template <typename Promise, typename Awaitable> auto await_differently_on(std::coroutine_handle<Promise> current, Awaitable && object) {
		(void)current;
		return std::forward<Awaitable>(object);
	}
};

template <typename T> auto get_scheduler() -> T & {
	static T global_scheduler{};
	return global_scheduler;
}

} // namespace co_curl

#endif
