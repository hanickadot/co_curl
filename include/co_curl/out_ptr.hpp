#ifndef CO_CURL_OUT_PTR_HPP
#define CO_CURL_OUT_PTR_HPP

namespace co_curl {

struct non_owning {
	constexpr void operator()(auto *) noexcept { }
};

template <typename T, typename Destroy = non_owning> struct out_ptr {
	T * ptr{nullptr};

	constexpr T ** ref() noexcept {
		return &ptr;
	}

	explicit constexpr operator T **() noexcept {
		return ref();
	}

	explicit constexpr operator bool() const noexcept {
		return ptr != nullptr;
	}

	constexpr auto convert_to_unique() noexcept {
		return std::unique_ptr<T>(std::exchange(ptr, nullptr));
	}

	template <typename O> constexpr auto cast_to() noexcept {
		return O(ptr);
	}

	constexpr ~out_ptr() {
		if (ptr) {
			Destroy{}(ptr);
		}
	}
};

} // namespace co_curl

#endif
