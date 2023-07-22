#include "easy.hpp"
#include "out_ptr.hpp"
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

void co_curl::easy_handle::url(const char * u) {
	curl_easy_setopt(native_handle, CURLOPT_URL, u);
}

void co_curl::easy_handle::verbose(bool enable) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_VERBOSE, static_cast<long>(enable));
}

void co_curl::easy_handle::pipewait(bool enable) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_PIPEWAIT, static_cast<long>(enable));
}

void co_curl::easy_handle::write_function(size_t (*f)(char *, size_t, size_t, void *)) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_WRITEFUNCTION, f);
}

void co_curl::easy_handle::write_data(void * udata) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_WRITEDATA, udata);
}

auto co_curl::easy_handle::get_content_type() const noexcept -> std::optional<std::string_view> {
	out_ptr<char, non_owning> out{};

	if (CURLE_OK != curl_easy_getinfo(native_handle, CURLINFO_CONTENT_TYPE, out.ref())) {
		return std::nullopt;
	}

	if (!out) {
		return std::nullopt;
	}

	return out.cast_to<std::string_view>();
}

auto co_curl::easy_handle::get_content_length() const noexcept -> std::optional<size_t> {
	curl_off_t cl{0};

	if (CURLE_OK != curl_easy_getinfo(native_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl)) {
		return std::nullopt;
	}

	return static_cast<size_t>(cl);
}

auto co_curl::easy_handle::get_response_code() const noexcept -> unsigned {
	long rc{0};

	curl_easy_getinfo(native_handle, CURLINFO_RESPONSE_CODE, &rc);

	return static_cast<unsigned>(rc);
}
