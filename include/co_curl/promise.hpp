#ifndef CO_CURL_PROMISE_HPP
#define CO_CURL_PROMISE_HPP

#include "concepts.hpp"
#include "scheduler.hpp"
#include <exception>
#include <optional>
#include <stdexcept>
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

template <typename R, typename Scheduler> struct promise_type: internal::promise_return<R> {
	using scheduler_type = Scheduler;

	scheduler_type & scheduler;
	std::coroutine_handle<> awaiter{};

	// construct my promise from me
	constexpr auto get_return_object() noexcept { return self(); }

	// we can provide the scheduler as coroutine argument...
	promise_type(scheduler_type & sch = co_curl::get_scheduler<scheduler_type>(), auto &&...) noexcept: scheduler{sch} { }

	// all my promises are immediate
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
	template <typename T> constexpr decltype(auto) await_transform(T && in) {
		return std::forward<T>(in);
	}

	constexpr auto await_transform(co_curl::perform perf) {
		// transform all easy_curl performs into lazy multi-performs
		return co_curl::perform_later(scheduler, perf.handle);
	}

	void add_awaiting(std::coroutine_handle<> other) {
		if (awaiter) {
			scheduler.add_additional_awaiter(self(), other);
		} else {
			awaiter = other;
		}
	}

	void remove_awaiting(std::coroutine_handle<> other) {
		if (awaiter == nullptr) {
			// nothing
		} else if (awaiter == other) {
			awaiter = {};
		} else {
			scheduler.remove_additional_awaiter(self(), other);
		}
	}

	auto someone_is_waiting_on_me(std::coroutine_handle<> other) -> std::coroutine_handle<> {
		add_awaiting(other);
		return scheduler.suspend();
	}
};

template <typename R, typename Scheduler = co_curl::default_scheduler> struct promise {
	using promise_type = co_curl::promise_type<R, Scheduler>;
	using handle_type = std::coroutine_handle<promise_type>;
	using return_type = R;

	handle_type handle{};

	promise(handle_type h) noexcept: handle{h} { }

	promise(const promise &) = delete;
	promise(promise && other) noexcept: handle{std::exchange(other.handle, nullptr)} { }

	promise & operator=(const promise &) = delete;
	promise & operator=(promise && other) noexcept {
		std::swap(handle, other.handle);
		return *this;
	}

	~promise() noexcept {
		if (handle) {
			handle.destroy();
		}
	}

	struct rvalue_awaiter {
		promise & t;

		bool await_ready() const noexcept {
			return t.handle.done();
		}

		template <typename T> auto await_suspend(std::coroutine_handle<T> awaiter) const {
			return t.handle.promise().someone_is_waiting_on_me(awaiter);
		}

		decltype(auto) await_resume() const {
			return t.handle.promise().result.move();
		}
	};

	struct lvalue_awaiter {
		promise & t;

		bool await_ready() const noexcept {
			return t.handle.done();
		}

		template <typename T> auto await_suspend(std::coroutine_handle<T> awaiter) const {
			return t.handle.promise().someone_is_waiting_on_me(awaiter);
		}

		decltype(auto) await_resume() const {
			return t.handle.promise().result.ref();
		}
	};

	struct const_lvalue_awaiter {
		const promise & t;

		bool await_ready() const noexcept {
			return t.handle.done();
		}

		template <typename T> auto await_suspend(std::coroutine_handle<T> awaiter) const {
			return t.handle.promise().someone_is_waiting_on_me(awaiter);
		}

		decltype(auto) await_resume() const {
			return t.handle.promise().result.cref();
		}
	};

	bool await_ready() const noexcept {
		assert(handle != nullptr);
		return handle.done();
	}

	template <typename T> auto await_suspend(std::coroutine_handle<T> awaiter) const {
		return handle.promise().someone_is_waiting_on_me(awaiter);
	}

	// support for moving out!
	auto operator co_await() & {
		return lvalue_awaiter{*this};
	}

	auto operator co_await() const & {
		return const_lvalue_awaiter{*this};
	}

	auto operator co_await() && {
		return rvalue_awaiter{*this};
	}

	auto operator co_await() const && = delete;

	// access
	decltype(auto) get() & {
		return handle.promise().result.ref();
	}

	template <typename Y> requires(std::same_as<Y, R> && !std::same_as<R, void>) operator Y &() & {
		return handle.promise().result.ref();
	}

	decltype(auto) get() const & {
		return handle.promise().result.cref();
	}

	template <typename Y> requires(std::same_as<Y, R> && !std::same_as<R, void>) operator const Y &() const & {
		return handle.promise().result.cref();
	}

	decltype(auto) get() && {
		return handle.promise().result.move();
	}

	template <typename Y> requires(std::same_as<Y, R> && !std::same_as<R, void>) operator Y() && {
		return handle.promise().result.move();
	}

	void get() const && = delete;

	// support for streaming
	template <typename T> friend auto operator<<(std::basic_ostream<T> & os, const promise & t) -> std::basic_ostream<T> & requires ostreamable<R, T> {
		return os << static_cast<const R &>(t);
	}
};

template <typename R, typename Scheduler = co_curl::default_scheduler> using task = promise<R, Scheduler>;

} // namespace co_curl

#endif
