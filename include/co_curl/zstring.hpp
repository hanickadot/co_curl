#ifndef CO_CURL_ZSTRING_HPP
#define CO_CURL_ZSTRING_HPP

namespace co_curl {

template <typename CharT, typename Deleter> struct zstring {
	const CharT * ptr;
	size_t sz;

	explicit constexpr zstring(const char * p) noexcept: ptr{p}, sz{std::char_traits<CharT>::length(ptr)} { }
	zstring(const zstring &) = delete;
	constexpr zstring(zstring && other) noexcept: ptr{std::exchange(other.ptr, nullptr)}, sz{other.sz} { }
	constexpr ~zstring() noexcept {
		if (ptr) {
			Deleter{}(ptr);
		}
	}

	zstring & operator=(const zstring &) = delete;
	constexpr zstring & operator=(zstring && other) noexcept {
		std::swap(ptr, other.ptr);
		std::swap(sz, other.sz);
		return *this;
	}

	constexpr operator std::string_view() const noexcept {
		return {ptr, sz};
	}
};

} // namespace co_curl

#endif
