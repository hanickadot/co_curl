#ifndef CO_CURL_MULTI_HPP
#define CO_CURL_MULTI_HPP

#include "easy.hpp"
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <chrono>
#include <coroutine>
#include <cstddef>

using CURLM = void;
using CURL = void;

namespace co_curl {

struct multi_handle {
	CURLM * native_handle;

	// constructors
	explicit constexpr multi_handle(std::nullptr_t) noexcept: native_handle{nullptr} { }
	multi_handle();
	multi_handle(const multi_handle &) = delete;
	constexpr multi_handle(multi_handle && other) noexcept: native_handle{std::exchange(other.native_handle, nullptr)} { }

	// destructor
	~multi_handle() noexcept;

	// assignments
	multi_handle & operator=(const multi_handle &) = delete;

	constexpr multi_handle & operator=(multi_handle && rhs) noexcept {
		std::swap(native_handle, rhs.native_handle);
		return *this;
	}

	// utility
	void add_handle(easy_handle & handle);
	bool remove_handle(easy_handle & handle) noexcept;
	bool remove_handle(CURL * handle) noexcept;

	bool sync_perform(unsigned & running_handles);
	auto sync_perform() -> std::optional<unsigned>;

	bool poll(std::chrono::milliseconds timeout = std::chrono::milliseconds{100}) noexcept;

	// API used by scheduler
	struct finished {
		CURL * handle;
		int code;
	};

	struct message: std::variant<finished> {
		auto visit(auto && cb) const noexcept {
			return std::visit(cb, *this);
		}
	};

	auto info_read(unsigned & msg_remaining) noexcept -> std::optional<message>;
	auto info_read() noexcept -> std::optional<message> {
		[[maybe_unused]] unsigned remaining = 0;
		return info_read(remaining);
	}

	// options
};

template <typename Scheduler> struct perform_later {
	Scheduler & scheduler;
	easy_handle & easy;
	result & result_ref;

	perform_later(Scheduler & sch, easy_handle & h) noexcept: scheduler{sch}, easy{h}, result_ref{scheduler.waiting.code} { }

	constexpr bool await_ready() noexcept {
		return false;
	}

	template <typename T> constexpr auto await_suspend(std::coroutine_handle<T> caller) {
		return scheduler.schedule_later(caller, easy);
	}

	constexpr result await_resume() const noexcept {
		return result_ref;
	}
};

} // namespace co_curl

#endif