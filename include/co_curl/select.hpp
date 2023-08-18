#ifndef CO_CURL_SELECT_HPP
#define CO_CURL_SELECT_HPP

#include "all.hpp"
#include <ranges>
#include <tuple>
#include <vector>

namespace co_curl {

template <typename... Ts> struct select_tuple_awaitor {
	std::tuple<Ts...> promises;
	std::coroutine_handle<> awaiting{};

	using result_type = std::common_type_t<typename std::remove_cvref_t<Ts>::return_type...>;

	static constexpr auto index = std::make_index_sequence<sizeof...(Ts)>();

	select_tuple_awaitor(Ts... rng) noexcept: promises(std::forward<decltype(rng)>(rng)...) { }

	template <typename CB> bool any(CB && cb) const {
		return [&]<size_t... Idx>(std::index_sequence<Idx...>) { return ((bool)cb(std::get<Idx>(promises)) || ... || false); }(index);
	}

	bool await_ready() const noexcept {
		// check if any is already ready
		return any([](auto && promise) -> bool { return promise.await_ready(); });
	}

	template <typename R, typename Scheduler> auto await_suspend(std::coroutine_handle<co_curl::promise_type<R, Scheduler>> h) noexcept {
		awaiting = h;
		// any of these coroutines if will be finished will wake up awaiting coroutine `h`
		for_each([&](auto && promise) { promise.handle.promise().add_awaiting(awaiting); });

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

	template <typename CB> void for_each(CB && cb) {
		[&]<size_t... Idx>(std::index_sequence<Idx...>) { ((void)cb(std::get<Idx>(promises)), ...); }(index);
	}

	auto await_resume() noexcept -> result_type {
		// find the ready one and return it's output
		for_each([&](auto && promise) { promise.handle.promise().remove_awaiting(awaiting); });

		return recursive_get();
	}
};

// for now only parameter packs ... ranges later
auto select(type_is_task auto &&... promises) {
	return co_curl::select_tuple_awaitor<decltype(promises)...>(std::forward<decltype(promises)>(promises)...);
}

} // namespace co_curl

#endif
