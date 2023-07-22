#ifndef CO_CURL_TASK_HPP
#define CO_CURL_TASK_HPP

#include "scheduler.hpp"
#include <optional>
#include <cassert>
#include <concepts>
#include <coroutine>

namespace co_curl {

namespace internal {
	template <typename T> struct promise_return {
		std::optional<T> result{std::nullopt};

		constexpr void return_value(std::convertible_to<T> auto && r) {
			result = T(std::forward<decltype(r)>(r));
		}

		constexpr T & get_result() & noexcept {
			assert(result.has_value());
			return *result;
		}

		constexpr T get_result() && noexcept {
			assert(result.has_value());
			return std::move(*result);
		}

		auto unhandled_exception() noexcept {
			std::terminate(); // todo remove
		}
	};

	template <> struct promise_return<void> {
		constexpr void return_void() noexcept { }

		constexpr void get_result() noexcept { }

		auto unhandled_exception() noexcept {
			std::terminate(); // todo remove
		}
	};

} // namespace internal

template <typename Promise> concept promise_with_scheduler = requires(Promise & promise) {
	{ promise.awaiter } -> std::convertible_to<std::coroutine_handle<>>;
	{ promise.scheduler };
};

template <promise_with_scheduler Promise> struct never_suspend_and_mark_root {
	Promise & promise;

	never_suspend_and_mark_root(Promise & pr) noexcept: promise{pr} {
		promise.scheduler.start();
	}

	constexpr bool await_ready() const noexcept { return true; }
	constexpr void await_resume() const noexcept { }
	constexpr void await_suspend(std::coroutine_handle<>) {
		// will never be called
		assert(false);
	}
};

template <promise_with_scheduler Promise> struct suspend_and_schedule_next {
	Promise & promise;

	suspend_and_schedule_next(Promise & pr) noexcept: promise{pr} {
		promise.scheduler.finish();
	}

	constexpr bool await_ready() const noexcept { return false; }
	constexpr void await_resume() const noexcept { }
	constexpr auto await_suspend(std::coroutine_handle<Promise>) {
		// TODO: if there is multiple awaiters, we want to let know scheduler

		if (promise.awaiter) {
			return promise.scheduler.schedule_now(promise.awaiter);
		}

		return promise.scheduler.schedule_next();
	}
};

template <typename R, typename Scheduler> struct promise_type: internal::promise_return<R> {
	using scheduler_type = Scheduler;

	scheduler_type & scheduler;
	std::coroutine_handle<> awaiter{};

	// construct my task from me
	constexpr auto get_return_object() noexcept { return self(); }

	// we can provide the scheduler as coroutine argument...
	promise_type(scheduler_type & sch = co_curl::get_scheduler<scheduler_type>(), auto &&...) noexcept: scheduler{sch} { }

	// all my tasks are immediate
	constexpr auto initial_suspend() noexcept {
		return never_suspend_and_mark_root(*this);
	};

	constexpr auto final_suspend() noexcept {
		return suspend_and_schedule_next(*this);
	};

	constexpr auto self() noexcept -> std::coroutine_handle<promise_type> {
		return std::coroutine_handle<promise_type>::from_promise(*this);
	}

	// pass all transformations to scheduler
	template <typename T> constexpr auto await_transform(T && something) {
		return scheduler.await_differently_on(self(), something);
	}

	constexpr auto get_awaiter() const noexcept {
		return awaiter;
	}

	template <typename T> auto someone_is_waiting_on_me(std::coroutine_handle<T> other) -> std::coroutine_handle<> {
		if (awaiter) {
			// scheduler.set_additional_awaiters(self(), other);
			// scheduler.set_additional_awaiters(self(), other);
			assert(false);
		} else {
			awaiter = other;
		}

		return scheduler.do_something_else();
	}
};

template <typename R, typename Scheduler = co_curl::default_scheduler> struct task {
	using promise_type = co_curl::promise_type<R, Scheduler>;
	using handle_type = std::coroutine_handle<promise_type>;

	handle_type handle{};

	task(handle_type h) noexcept: handle{h} { }

	void finish() {
		// if (!handle.done()) {
		//	std::cout << "\n\n\nFINISH:\n";
		// }

		// std::cout << "\n\n\nFINISH\n";
		// while (!handle.done()) {
		//	const auto next_handle = handle.promise().scheduler.loop_to_finish();
		//	assert(next_handle);
		//	next_handle.resume();
		// }
	}

	auto get_result_without_finishing() & noexcept {
		assert(handle != nullptr);
		assert(handle.done());
		return handle.promise().get_result();
	}

	auto get_result_without_finishing() && noexcept {
		assert(handle != nullptr);
		assert(handle.done());
		return std::move(handle.promise()).get_result();
	}

	auto get_result() & noexcept {
		finish();
		return get_result_without_finishing();
	}

	auto get_result() && noexcept {
		finish();
		return get_result_without_finishing();
	}

	operator R() & noexcept {
		return get_result();
	}

	operator R() && noexcept {
		return get_result();
	}

	bool await_ready() const noexcept {
		assert(handle != nullptr);
		return handle.done();
	}

	template <typename T> auto await_suspend(std::coroutine_handle<T> awaiter) {
		return handle.promise().someone_is_waiting_on_me(awaiter);
	}

	auto await_resume() & noexcept {
		return get_result_without_finishing();
	}

	auto await_resume() && noexcept {
		return get_result_without_finishing();
	}
};

} // namespace co_curl

#endif
