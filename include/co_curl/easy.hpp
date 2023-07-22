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

struct perform;

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
	bool sync_perform() noexcept;
	auto perform() noexcept -> co_curl::perform;

	// setters
	void url(const char * u);
	void url(const std::string & u) { url(u.c_str()); }

	void verbose(bool enable = true) noexcept;
	void pipewait(bool enable = true) noexcept;

	void write_function(size_t (*)(char *, size_t, size_t, void *)) noexcept;
	void write_data(void *) noexcept;

	// getters
	auto get_content_type() const noexcept -> std::optional<std::string_view>;
	auto get_content_length() const noexcept -> std::optional<size_t>;
	auto get_response_code() const noexcept -> unsigned;

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
};

} // namespace co_curl

#endif
