#include "url.hpp"
#include <curl/curl.h>

co_curl::url::url(): handle{curl_url()} { }

co_curl::url::url(const char * a, const char * b): url() {
	operator=(a);
	operator=(b);
}

co_curl::url::url(const char * cstr): url() {
	operator=(cstr);
}

co_curl::url::url(const url & other): handle{curl_url_dup(other.handle)} { }

co_curl::url::~url() noexcept {
	curl_url_cleanup(handle);
}

co_curl::url & co_curl::url::remove_fragment() {
	curl_url_set(handle, CURLUPART_FRAGMENT, nullptr, 0);
	return *this;
}

co_curl::url & co_curl::url::remove_query() {
	curl_url_set(handle, CURLUPART_QUERY, nullptr, 0);
	return *this;
}

co_curl::url & co_curl::url::operator=(const char * cstr) {
	curl_url_set(handle, CURLUPART_URL, cstr, 0);
	return *this;
}

static std::optional<std::string> get_url_as_string(CURLU * handle, CURLUPart part) {
	char * ptr{nullptr};
	curl_url_get(handle, part, &ptr, 0);

	if (ptr == nullptr) {
		return std::nullopt;
	}

	return std::string{ptr};
}

std::optional<std::string> co_curl::url::get() const {
	return get_url_as_string(handle, CURLUPART_URL);
}
std::optional<std::string> co_curl::url::host() const {
	return get_url_as_string(handle, CURLUPART_HOST);
}
std::optional<std::string> co_curl::url::path() const {
	return get_url_as_string(handle, CURLUPART_PATH);
}