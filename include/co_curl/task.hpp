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
	};

	template <> struct promise_return<void> {
		constexpr void return_void() noexcept { }

		constexpr void get_result() noexcept { }
	};
} // namespace internal

template <typename R, typename Scheduler = co_curl::default_scheduler> struct task {
	struct promise_type: internal::promise_return<R> {
		using scheduler_type = Scheduler;

		scheduler_type & scheduler;
		std::coroutine_handle<> awaiter{};

		promise_type(scheduler_type & sch = co_curl::get_scheduler<scheduler_type>(), auto &&...) noexcept: scheduler{sch} { }

		constexpr auto initial_suspend() noexcept -> std::suspend_never { return {}; };
		constexpr auto final_suspend() noexcept {
			struct suspend_and_jump_to_next {
				promise_type & promise;

				constexpr bool await_ready() noexcept { return false; }
				auto await_suspend(std::coroutine_handle<promise_type> self) noexcept {
					return promise.scheduler.coroutine_finished(promise.awaiter);
				}
				constexpr void await_resume() noexcept { }
			};
			return suspend_and_jump_to_next{*this};
		};
		constexpr auto get_return_object() noexcept { return self(); }
		auto unhandled_exception() noexcept {
			std::terminate(); // todo remove
		}

		constexpr auto self() noexcept -> std::coroutine_handle<promise_type> {
			return std::coroutine_handle<promise_type>::from_promise(*this);
		}

		template <typename Awaitee> struct awaiting_on_task_from_task {
			Awaitee awaitee;
			bool await_ready() noexcept {
				return awaitee.await_ready();
			}

			auto await_suspend(std::coroutine_handle<>) {
				// just go to previous coroutine
				return std::noop_coroutine();
			}

			auto await_resume() noexcept {
				return awaitee.get_result();
			}
		};

		// template <typename R2> constexpr auto await_transform(task<R2, Scheduler> & t) {
		//	// TODO do more generic for other awaitable types associated with our scheduler
		//	return awaiting_on_task_from_task<task<R2, Scheduler> &>{t};
		// }
		//
		// template <typename R2> constexpr auto await_transform(task<R2, Scheduler> && t) {
		//	// TODO do more generic for other awaitable types associated with our scheduler
		//	return awaiting_on_task_from_task<task<R2, Scheduler> &&>{std::forward<decltype(t)>(t)};
		// }

		auto set_awaiter_and_return_next(std::coroutine_handle<> other) -> std::coroutine_handle<> {
			if (awaiter) {
				// scheduler.set_additional_awaiters(self(), other);
				// scheduler.set_additional_awaiters(self(), other);
				assert(false);
			} else {
				awaiter = other;
			}

			return scheduler.coroutine_awaiting();
		}
	};

	using handle_type = std::coroutine_handle<promise_type>;

	handle_type handle{};

	task(handle_type h) noexcept: handle{h} { }

	auto get_result() & noexcept {
		assert(handle != nullptr);
		assert(handle.done());
		return handle.promise().get_result();
	}

	auto get_result() && noexcept {
		assert(handle != nullptr);
		assert(handle.done());
		return std::move(handle.promise()).get_result();
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

	auto await_suspend(std::coroutine_handle<> awaiter) {
		return handle.promise().set_awaiter_and_return_next(awaiter);
	}

	auto await_resume() & noexcept {
		return get_result();
	}

	auto await_resume() && noexcept {
		return get_result();
	}
};

} // namespace co_curl

#endif
