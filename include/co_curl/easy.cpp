#include "easy.hpp"
#include <curl/curl.h>

co_curl::easy_handle::easy_handle(): native_handle{curl_easy_init()} { }

co_curl::easy_handle::~easy_handle() noexcept {
	if (native_handle) {
		curl_easy_cleanup(native_handle);
	}
}

auto co_curl::easy_handle::duplicate() const -> easy_handle {
	auto out = easy_handle{nullptr};
	out.native_handle = curl_easy_duphandle(native_handle);
	return out;
}

void co_curl::easy_handle::reset() noexcept {
	curl_easy_reset(native_handle);
}

bool co_curl::easy_handle::sync_perform() noexcept {
	// TODO do something with return code
	return CURLE_OK == curl_easy_perform(native_handle);
}

void co_curl::easy_handle::set_url(const char * url) {
	curl_easy_setopt(native_handle, CURLOPT_URL, url);
}

void co_curl::easy_handle::set_verbose(bool enable) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_VERBOSE, static_cast<long>(enable));
}

void co_curl::easy_handle::set_write_function(size_t (*f)(char *, size_t, size_t, void *)) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_WRITEFUNCTION, f);
}

void co_curl::easy_handle::set_write_data(void * udata) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_WRITEDATA, udata);
}
