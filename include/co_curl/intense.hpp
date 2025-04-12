#ifndef CO_CURL_INSTENSE_HPP
#define CO_CURL_INSTENSE_HPP

#include "promise.hpp"
#include "scheduler.hpp"
#include <thread>
#include <coroutine>

namespace co_curl {

struct start_thread_t {
	constexpr bool await_ready() const noexcept { return false; }
	template <typename R, typename Scheduler> constexpr auto await_suspend(std::coroutine_handle<co_curl::promise_type<R, Scheduler>> current) const noexcept -> std::coroutine_handle<> {
		// tell scheduler we are starting a new thread, and continue with something else
		return current.promise().scheduler.switch_thread(current);
	}
	constexpr void await_resume() const noexcept { }
};

struct finish_thread_t {
	// TODO check if current thread is in thread
	constexpr bool await_ready() const noexcept { return false; }
	template <typename R, typename Scheduler> constexpr auto await_suspend(std::coroutine_handle<co_curl::promise_type<R, Scheduler>> current) const noexcept -> std::coroutine_handle<> {
		// let scheduler know it should pick this up later
		if ((current.promise().scheduler.finish_thread(current, true))) {
			return std::noop_coroutine();
		} else {
			return current;
		}
	}
	constexpr void await_resume() const noexcept { }
};

constexpr auto start_thread = start_thread_t{};
constexpr auto move_to_new_thread = start_thread_t{};
constexpr auto finish_thread = finish_thread_t{};
constexpr auto move_back_to_main_thread = finish_thread_t{};

} // namespace co_curl

#endif
