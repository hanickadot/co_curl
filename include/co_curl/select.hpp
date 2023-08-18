#ifndef CO_CURL_SELECT_HPP
#define CO_CURL_SELECT_HPP

#include "all.hpp"
#include <ranges>
#include <tuple>
#include <vector>

namespace co_curl {

template <range_of_tasks Range> struct select_awaitor {
	Range range;
	std::coroutine_handle<> awaiting{};

	select_awaitor(Range rng) noexcept: range{rng} { }
	select_awaitor(std::same_as<std::ranges::range_value_t<Range>> auto... rng) noexcept: range{std::move(rng)...} { }

	bool await_ready() const noexcept {
		// check if any is already ready
		for (auto & task: range) {
			if (task.await_ready()) {
				return true;
			}
		}
		return false;
	}

	template <typename R, typename Scheduler> auto await_suspend(std::coroutine_handle<co_curl::promise_type<R, Scheduler>> h) noexcept {
		awaiting = h;
		// any of these coroutines if will be finished will wake up awaiting coroutine `h`
		for (auto & task: range) {
			task.handle.promise().add_awaiting(awaiting);
		}

		// ask scheduler what to do next, this will be awaken when some of the coroutines is finished...
		return h.promise().scheduler.suspend();
	}

	auto await_resume() noexcept {
		// find the ready one and return it's output
		for (auto & task: range) {
			task.handle.promise().remove_awaiting(awaiting);
		}

		// iterate over to look for finished one
		for (auto & task: range) {
			if (task.await_ready()) {
				return task.operator co_await().await_resume();
			}
		}

		std::terminate();
	}
};

template <typename> struct identify;

template <range_of_tasks R> auto select(R && tasks) {
	return co_curl::select_awaitor<R>(std::forward<R>(tasks));
}

template <typename Ts, typename Scheduler> auto select(co_curl::promise<Ts, Scheduler> task, std::same_as<co_curl::promise<Ts, Scheduler>> auto... tail) {
	using array_type = std::array<co_curl::promise<Ts, Scheduler>, sizeof...(tail) + 1u>;
	return co_curl::select_awaitor<array_type>(std::move(task), std::move(tail)...);
}

} // namespace co_curl

#endif
