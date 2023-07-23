#ifndef CO_CURL_TASK_HPP
#define CO_CURL_TASK_HPP

#include "concepts.hpp"
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

		constexpr T & get() & noexcept {
			assert(result.has_value());
			return *result;
		}

		constexpr T get() && noexcept {
			assert(result.has_value());
			return std::move(*result);
		}

		auto unhandled_exception() noexcept {
			std::terminate(); // todo remove
		}
	};

	template <> struct promise_return<void> {
		constexpr void return_void() noexcept { }

		constexpr void get() noexcept { }

		auto unhandled_exception() noexcept {
			std::terminate(); // todo remove
		}
	};

} // namespace internal

template <typename Promise> concept promise_with_scheduler = requires(Promise & promise) {
	{ promise.awaiter } -> std::convertible_to<std::coroutine_handle<>>;
	{ promise.scheduler };
};

template <promise_with_scheduler Promise> struct suspend_and_schedule_next {
	Promise & promise;

	suspend_and_schedule_next(Promise & pr) noexcept: promise{pr} { }

	constexpr bool await_ready() const noexcept { return false; }
	constexpr void await_resume() const noexcept { }
	constexpr auto await_suspend(std::coroutine_handle<Promise>) {
		// TODO: if there is multiple awaiters, we want to let know scheduler
		promise.scheduler.wakeup_coroutines_waiting_for(promise.self());
		return promise.scheduler.select_next_coroutine(promise.awaiter);
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
		scheduler.start();
		return std::suspend_never();
	};

	constexpr auto final_suspend() noexcept {
		scheduler.finish();
		return suspend_and_schedule_next(*this);
	};

	constexpr auto self() noexcept -> std::coroutine_handle<promise_type> {
		return std::coroutine_handle<promise_type>::from_promise(*this);
	}

	// pass all transformations to scheduler
	template <typename T> constexpr decltype(auto) await_transform(T && something) {
		if constexpr (scheduler_transforms<Scheduler, promise_type &, T &&>) {
			return scheduler.transform(*this, something);
		} else {
			return std::forward<T>(something);
		}
	}

	constexpr auto await_transform(co_curl::perform perf) {
		// transform all easy_curl performs into lazy multi-performs
		return co_curl::perform_later(scheduler, perf.handle);
	}

	template <typename T> auto someone_is_waiting_on_me(std::coroutine_handle<T> other) -> std::coroutine_handle<> {
		if (awaiter) {
			return scheduler.suspend_additional_awaiter(self(), other);
			// scheduler.suspend_and_wait_fo(other);
		} else {
			awaiter = other;
			return scheduler.suspend();
		}
	}
};

template <typename R, typename Scheduler = co_curl::default_scheduler> struct task {
	using promise_type = co_curl::promise_type<R, Scheduler>;
	using handle_type = std::coroutine_handle<promise_type>;
	using return_type = R;

	handle_type handle{};

	task(handle_type h) noexcept: handle{h} { }

	task(const task &) = delete;
	task(task && other) noexcept: handle{std::exchange(other.handle, nullptr)} { }

	task & operator=(const task &) = delete;
	task & operator=(task && other) noexcept {
		std::swap(handle, other.handle);
		return *this;
	}

	~task() noexcept {
		if (handle) {
			handle.destroy();
		}
	}

	void finish() const {
		assert(handle != nullptr);
		if (!handle.done()) {
			handle.resume();
		}
	}

	auto get_without_finishing() & {
		assert(handle != nullptr);
		assert(handle.done());
		return handle.promise().get();
	}

	auto get_without_finishing() const & {
		assert(handle != nullptr);
		assert(handle.done());
		return handle.promise().get();
	}

	auto get_without_finishing() const && {
		assert(handle != nullptr);
		assert(handle.done());
		return std::move(handle.promise()).get();
	}

	auto get_without_finishing() && {
		assert(handle != nullptr);
		assert(handle.done());
		return std::move(handle.promise()).get();
	}

	auto get() & noexcept {
		finish();
		return get_without_finishing();
	}

	auto get() const & noexcept {
		finish();
		return get_without_finishing();
	}

	auto get() && noexcept {
		finish();
		return get_without_finishing();
	}

	auto get() const && noexcept {
		finish();
		return get_without_finishing();
	}

	operator R() & noexcept {
		return get();
	}

	operator R() && noexcept {
		return get();
	}

	bool await_ready() const noexcept {
		assert(handle != nullptr);
		return handle.done();
	}

	template <typename T> auto await_suspend(std::coroutine_handle<T> awaiter) const {
		return handle.promise().someone_is_waiting_on_me(awaiter);
	}

	auto await_resume() & noexcept {
		return get_without_finishing();
	}

	auto await_resume() && noexcept {
		return get_without_finishing();
	}

	auto await_resume() const & noexcept {
		return get_without_finishing();
	}

	auto await_resume() const && noexcept {
		return get_without_finishing();
	}

	// support for streaming
	friend std::ostream & operator<<(std::ostream & os, const task & t) requires ostreamable<R> {
		return os << t.get();
	}
};

} // namespace co_curl

#endif
