#ifndef CO_CURL_EASY_HPP
#define CO_CURL_EASY_HPP

#include "list.hpp"
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <cassert>
#include <coroutine>
#include <cstddef>

using CURL = void;

namespace co_curl {

struct perform;

struct result {
	int code;

	explicit operator bool() const noexcept;

	operator std::string_view() const noexcept;
	const char * c_str() const noexcept;
	friend inline std::ostream & operator<<(std::ostream & os, result r) {
		return os << r.c_str();
	}
};

struct easy_handle {
	CURL * native_handle;

	// constructors
	explicit constexpr easy_handle(std::nullptr_t) noexcept: native_handle{nullptr} { }
	easy_handle();
	easy_handle(const easy_handle &) = delete;
	constexpr easy_handle(easy_handle && other) noexcept: native_handle{std::exchange(other.native_handle, nullptr)} { }

	// helper constructors
	easy_handle(const char * u): easy_handle() {
		url(u);
	}

	easy_handle(const std::string & u): easy_handle() {
		url(u);
	}

	// destructor
	~easy_handle() noexcept;

	// assignments
	easy_handle & operator=(const easy_handle &) = delete;

	constexpr easy_handle & operator=(easy_handle && rhs) noexcept {
		std::swap(native_handle, rhs.native_handle);
		return *this;
	}

	// utility
	void reset() noexcept;
	easy_handle duplicate() const;
	result sync_perform() noexcept;
	auto perform() noexcept -> co_curl::perform;

	// setters
	void url(const char * u);
	void url(const std::string & u) { url(u.c_str()); }

	void verbose(bool enable = true) noexcept;
	void pipewait(bool enable = true) noexcept;
	void fresh_connect(bool enable = true) noexcept;
	void upload(bool enable = true) noexcept;

	void infile_size(size_t size) noexcept;
	void prequote(list & lst) noexcept;
	void quote(list & lst) noexcept;
	void postquote(list & lst) noexcept;

	void username(const char *) noexcept;
	void password(const char *) noexcept;

	// getters
	auto get_content_type() const noexcept -> std::optional<std::string_view>;
	auto get_content_length() const noexcept -> std::optional<size_t>;
	auto get_response_code() const noexcept -> unsigned;

	// write/read
	void write_function(size_t (*)(char *, size_t, size_t, void *)) noexcept;
	void write_data(void *) noexcept;
	void read_function(size_t (*)(char *, size_t, size_t, void *)) noexcept;
	void read_data(void *) noexcept;

	// extended callbacks
	template <typename T> void write_callback(std::invocable<T> auto & f) {
		using value_type = typename T::value_type;
		using fnc_t = decltype(f);

		const auto helper = +[](char * in, size_t, size_t nmemb, void * udata) -> size_t {
			const auto data = T(reinterpret_cast<const value_type *>(const_cast<const char *>(in)), nmemb);
			fnc_t & f = *static_cast<fnc_t *>(udata);
			try {
				f(data);
				return nmemb;
			} catch (...) {
				return size_t(-1);
			}
		};

		write_function(helper);
		write_data(&f);
	}

	void write_callback(std::invocable<std::span<const std::string_view>> auto & f) {
		write_callback<std::string_view>(f);
	}

	void write_callback(std::invocable<std::span<const std::byte>> auto & f) {
		write_callback<std::span<const std::byte>>(f);
	}

	template <typename T> void write_into(T & out) {
		using value_type = typename T::value_type;
		static_assert(sizeof(value_type) == sizeof(char));

		write_function(+[](char * in, size_t, size_t nmemb, void * udata) -> size_t {
			T & o = *static_cast<T *>(udata);

			const auto * ptr = reinterpret_cast<const value_type *>(const_cast<const char *>(in));

			std::copy(ptr, ptr + nmemb, std::back_inserter(o));

			return nmemb;
		});

		write_data(&out);
	}

	void write_nowhere() noexcept {
		write_function(+[](char *, size_t, size_t nmemb, void *) -> size_t { return nmemb; });
	}

	// reads

	template <typename T, typename F> void read_callback(F & f) requires(std::is_invocable_r_v<size_t, F &, T>) {
		using value_type = typename T::value_type;
		using fnc_t = F;

		const auto helper = +[](char * in, size_t, size_t nmemb, void * udata) -> size_t {
			const auto target = T(reinterpret_cast<value_type *>(in), nmemb);
			fnc_t & f = *static_cast<fnc_t *>(udata);
			try {
				return f(target);
			} catch (...) {
				return size_t(-1);
			}
		};

		read_function(helper);
		read_data(&f);
	}

	template <typename CB> void read_callback(CB & f) requires(std::is_invocable_r_v<size_t, CB &, std::span<std::byte>>) {
		read_callback<std::span<std::byte>>(f);
	}

	template <typename CB> void read_callback(CB & f) requires(std::is_invocable_r_v<size_t, CB &, std::span<char>>) {
		read_callback<std::span<char>>(f);
	}
};

struct perform {
	easy_handle & handle;
	result code{};

	constexpr perform(easy_handle & orig) noexcept: handle{orig} {
		assert(handle.native_handle != nullptr);
	}

	constexpr bool await_ready() noexcept {
		return false;
	}

	explicit operator bool() {
		return static_cast<bool>(handle.sync_perform());
	}

	template <typename T> constexpr auto await_suspend(std::coroutine_handle<T> caller) {
		code = handle.sync_perform();
		return caller; // resume current coroutine (I know I can do it with return bool(false), but this is more explicit)
	}

	constexpr result await_resume() const noexcept {
		return code;
	}
};

} // namespace co_curl

#endif
