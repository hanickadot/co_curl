#ifndef CO_CURL_EASY_HPP
#define CO_CURL_EASY_HPP

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <coroutine>
#include <cstddef>

using CURL = void;

namespace co_curl {

struct easy_handle {
	CURL * native_handle;

	// constructors
	explicit constexpr easy_handle(std::nullptr_t) noexcept: native_handle{nullptr} { }
	easy_handle();
	easy_handle(const easy_handle &) = delete;
	constexpr easy_handle(easy_handle && other) noexcept: native_handle{std::exchange(other.native_handle, nullptr)} { }

	// helper constructors
	easy_handle(const char * url): easy_handle() {
		set_url(url);
	}

	easy_handle(const std::string & url): easy_handle() {
		set_url(url);
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
	bool sync_perform() noexcept;

	// setters
	void set_url(const char * url);
	void set_url(const std::string & url) { set_url(url.c_str()); }

	void set_verbose(bool enable = true) noexcept;

	void set_write_function(size_t (*)(char *, size_t, size_t, void *)) noexcept;
	void set_write_data(void *) noexcept;

	template <typename T, std::invocable<T> Fnc> void set_write_callback(Fnc & f) {
		using value_type = typename T::value_type;

		const auto helper = +[](char * in, size_t, size_t nmemb, void * udata) -> size_t {
			const auto data = T(reinterpret_cast<const value_type *>(const_cast<const char *>(in)), nmemb);
			Fnc & f = *static_cast<Fnc *>(udata);
			try {
				f(data);
				return nmemb;
			} catch (...) {
				return size_t(-1);
			}
		};

		set_write_function(helper);
		set_write_data(&f);
	}

	template <std::invocable<std::string_view> Fnc> void set_write_callback(Fnc & f) {
		set_write_callback<std::string_view>(f);
	}

	template <std::invocable<std::span<const std::byte>> Fnc> void set_write_callback(Fnc & f) {
		set_write_callback<std::span<const std::byte>>(f);
	}

	// getters
};

struct perform {
	easy_handle & handle;
	bool result{};

	constexpr perform(easy_handle & orig) noexcept: handle{orig} {
		assert(handle.native_handle != nullptr);
	}

	constexpr bool await_ready() noexcept {
		return false;
	}

	template <typename T> constexpr auto await_suspend(std::coroutine_handle<T> caller) {
		result = handle.sync_perform();
		return caller; // resume current coroutine (I know I can do it with return bool(false), but this is more explicit)
	}

	constexpr int await_resume() const noexcept {
		return result;
	}

	struct lazy_perform {
		bool result{true}; // FIXME provide correct result

		constexpr bool await_ready() noexcept {
			return false;
		}

		template <typename T> constexpr auto await_suspend(std::coroutine_handle<T> caller) {
			return std::noop_coroutine();
		}

		constexpr int await_resume() const noexcept {
			return result;
		}
	};
};

} // namespace co_curl

#endif
