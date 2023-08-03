#ifndef CO_CURL_TASK_HPP
#define CO_CURL_TASK_HPP

#include "concepts.hpp"
#include "scheduler.hpp"
#include <optional>
#include <variant>
#include <cassert>
#include <concepts>
#include <coroutine>

namespace co_curl {

namespace internal {
	template <typename From, typename To> concept move_constructible = requires(From & from, To to) {
		{ To(std::move(from)) };
	};

	template <typename T> struct result_type: std::variant<std::monostate, T, std::exception_ptr> {
		using super = std::variant<std::monostate, T, std::exception_ptr>;
		using super::super;

		bool has_value() const noexcept {
			return super::index() == 1u;
		}

		bool has_exception() const noexcept {
			return super::index() == 2u;
		}

		void check_exception() {
			if (has_exception()) {
				std::rethrow_exception(*std::get_if<2>(this));
			}
		}

		T & ref() {
			check_exception();
			assert(has_value());
			return *std::get_if<1>(this);
		}

		const T & cref() {
			check_exception();
			assert(has_value());
			return *std::get_if<1>(this);
		}

		T move() {
			check_exception();
			assert(has_value());
			return std::move(*std::get_if<1>(this));
		}
	};

	struct empty_type { };

	template <> struct result_type<void>: std::variant<std::monostate, empty_type, std::exception_ptr> {
		using super = std::variant<std::monostate, empty_type, std::exception_ptr>;
		using super::super;

		bool has_value() const noexcept {
			return super::index() == 1u;
		}

		bool has_exception() const noexcept {
			return super::index() == 2u;
		}

		void check_exception() {
			if (has_exception()) {
				std::rethrow_exception(*std::get_if<2>(this));
			}
		}

		void ref() {
			check_exception();
		}

		void cref() {
			check_exception();
		}

		void move() {
			check_exception();
		}
	};

	template <typename T> struct promise_return {
		result_type<T> result{};

		template <move_constructible<T> Y> void return_value(Y && r) {
			// to avoid assign
			std::destroy_at(&result);
			new (&result) result_type<T>(std::in_place_type<T>, std::move(r));
		}

		auto unhandled_exception() noexcept {
			std::destroy_at(&result);
			new (&result) result_type<T>(std::in_place_type<std::exception_ptr>, std::current_exception());
		}
	};

	template <> struct promise_return<void> {
		result_type<void> result{};

		void return_void() noexcept {
			std::destroy_at(&result);
			new (&result) result_type<void>(std::in_place_type<empty_type>);
		}

		auto unhandled_exception() noexcept {
			std::destroy_at(&result);
			new (&result) result_type<void>(std::in_place_type<std::exception_ptr>, std::current_exception());
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
	constexpr auto await_suspend(std::coroutine_handle<Promise>) noexcept {
		// TODO: if there is multiple awaiters, we want to let know scheduler
		promise.scheduler.wakeup_coroutines_waiting_for(promise.self());
		return promise.scheduler.select_next_coroutine(promise.awaiter);
	}
};

template <typename T> struct awaiter_forward {
	std::remove_reference_t<T> & object;

	bool await_ready() const noexcept {
		return object.await_ready();
	}
	template <typename Y> auto await_suspend(std::coroutine_handle<Y> awaiter) const {
		return object.await_suspend(awaiter);
	}
	auto await_resume() const noexcept {
		if constexpr (std::is_lvalue_reference_v<T>) {
			if constexpr (std::is_const_v<T>) {
				return static_cast<const T &>(object).await_resume();
			} else {
				return static_cast<T &>(object).await_resume();
			}
		} else {
			static_assert(std::is_rvalue_reference_v<T>);
			static_assert(!std::is_const_v<T>);
			return std::move(object).await_resume();
		}
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
	template <typename T> constexpr auto await_transform(T && in) {
		return awaiter_forward<decltype(in)>(in);
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

	bool await_ready() const noexcept {
		assert(handle != nullptr);
		return handle.done();
	}

	template <typename T> auto await_suspend(std::coroutine_handle<T> awaiter) const {
		return handle.promise().someone_is_waiting_on_me(awaiter);
	}

	decltype(auto) get() & {
		return handle.promise().result.ref();
	}

	decltype(auto) await_resume() & {
		return handle.promise().result.ref();
	}

	template <typename Y> requires(std::same_as<Y, R> && !std::same_as<R, void>) operator Y &() & {
		return handle.promise().result.ref();
	}

	decltype(auto) get() const & {
		return handle.promise().result.cref();
	}

	decltype(auto) await_resume() const & {
		return handle.promise().result.cref();
	}

	template <typename Y> requires(std::same_as<Y, R> && !std::same_as<R, void>) operator const Y &() const & {
		return handle.promise().result.cref();
	}

	decltype(auto) get() && {
		return handle.promise().result.move();
	}

	decltype(auto) await_resume() && {
		return handle.promise().result.move();
	}

	template <typename Y> requires(std::same_as<Y, R> && !std::same_as<R, void>) operator Y() && {
		return handle.promise().result.move();
	}

	void get() const && = delete;
	void await_resume() const && = delete;

	// support for streaming
	template <typename T> friend auto operator<<(std::basic_ostream<T> & os, const task & t) -> std::basic_ostream<T> & requires ostreamable<R, T> {
		return os << static_cast<const R &>(t);
	}
};

} // namespace co_curl

#endif
