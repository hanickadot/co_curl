#ifndef CO_CURL_FUNCTION_HPP
#define CO_CURL_FUNCTION_HPP

#include "task.hpp"
#include <coroutine>

namespace co_curl {

template <typename T> struct function_promise_type: internal::promise_return<T> {
	constexpr auto initial_suspend() const noexcept { return std::suspend_never{}; }
	constexpr auto final_suspend() const noexcept { return std::suspend_always{}; }

	constexpr auto get_return_object() noexcept {
		return std::coroutine_handle<function_promise_type>::from_promise(*this);
	}

	auto yield_value(auto &&) noexcept = delete;
	auto await_transform(auto &&) noexcept = delete;
};

template <typename T> struct function {
	using promise_type = function_promise_type<T>;
	using handle_type = std::coroutine_handle<promise_type>;

	handle_type handle;

	function(handle_type h) noexcept: handle{h} { }

	~function() noexcept {
		handle.destroy();
	}

	operator T() && noexcept {
		return get();
	}

	T get() {
		assert(handle);
		assert(handle.done());
		return handle.promise().get();
	}
};

} // namespace co_curl

#endif