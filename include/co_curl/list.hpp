#ifndef CO_CURL_LIST_HPP
#define CO_CURL_LIST_HPP

#include <string>
#include <string_view>
#include <utility>

struct curl_slist;

namespace co_curl {

struct list {
	struct curl_slist * data{nullptr};

	constexpr list() noexcept = default;
	list(const list &) = delete;
	constexpr list(list && other) noexcept: data{std::exchange(other.data, nullptr)} { }

	list & operator=(const list &) = delete;
	list & operator=(list && other) noexcept {
		std::swap(data, other.data);
		return *this;
	}

	~list() noexcept;

	void append(const char * str);

	void append(const std::string & str) {
		append(str.c_str());
	}

	// void append(std::string_view in);
};

} // namespace co_curl

#endif
