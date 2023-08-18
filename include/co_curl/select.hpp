#ifndef CO_CURL_SELECT_HPP
#define CO_CURL_SELECT_HPP

#include "all.hpp"
#include <ranges>
#include <tuple>
#include <vector>

namespace co_curl {

template <typename Range> struct select_awaitor;

template <range_of_tasks Range> struct select_awaitor<Range> {
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

template <typename... Ts> struct select_awaitor<std::tuple<Ts...>> {
	std::tuple<Ts...> promises;
	std::coroutine_handle<> awaiting{};

	using result_type = std::common_type_t<typename std::remove_cvref_t<Ts>::return_type...>;

	static constexpr auto index = std::make_index_sequence<sizeof...(Ts)>();

	select_awaitor(Ts... rng) noexcept: promises(std::forward<decltype(rng)>(rng)...) { }

	bool await_ready() const noexcept {
		// check if any is already ready
		return [&]<size_t... Idx>(std::index_sequence<Idx...>) -> bool {
			return (std::get<Idx>(promises).await_ready() || ... || false);
		}(index);
	}

	template <typename R, typename Scheduler> auto await_suspend(std::coroutine_handle<co_curl::promise_type<R, Scheduler>> h) noexcept {
		awaiting = h;
		// any of these coroutines if will be finished will wake up awaiting coroutine `h`
		[&]<size_t... Idx>(std::index_sequence<Idx...>) {
			((void)(std::get<Idx>(promises).handle.promise().add_awaiting(awaiting)), ...);
			}(index);

		// ask scheduler what to do next, this will be awaken when some of the coroutines is finished...
		return h.promise().scheduler.suspend();
	}

	template <size_t Idx = 0> auto recursive_get() -> result_type {
		if constexpr (Idx >= sizeof...(Ts)) {
			std::terminate();
		} else {
			if (std::get<Idx>(promises).await_ready()) {
				return result_type{std::get<Idx>(promises).operator co_await().await_resume()};
			} else {
				return recursive_get<Idx + 1u>();
			}
		}
	}

	auto await_resume() noexcept -> result_type {
		// find the ready one and return it's output
		[&]<size_t... Idx>(std::index_sequence<Idx...>) {
			((void)(std::get<Idx>(promises).handle.promise().remove_awaiting(awaiting)), ...);
			}(index);

		return recursive_get();
	}
};

template <typename> struct identify;

template <range_of_tasks R> auto select(R && tasks) {
	return co_curl::select_awaitor<R>(std::forward<R>(tasks));
}

auto select(type_is_task auto &&... promises) {
	return co_curl::select_awaitor<std::tuple<decltype(promises)...>>(std::forward<decltype(promises)>(promises)...);
}

} // namespace co_curl

#endif
