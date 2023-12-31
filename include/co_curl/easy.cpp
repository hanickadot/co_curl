#include "easy.hpp"
#include "out_ptr.hpp"
#include <curl/curl.h>

co_curl::result::operator bool() const noexcept {
	return code == CURLE_OK;
}

co_curl::result::operator std::string_view() const noexcept {
	return std::string_view(c_str());
}

const char * co_curl::result::c_str() const noexcept {
	static_assert(sizeof(CURLcode) == sizeof(code));
	return curl_easy_strerror(static_cast<CURLcode>(code));
}

bool co_curl::result::is_partial_transfer() const noexcept {
	return code == CURLE_PARTIAL_FILE;
}

bool co_curl::result::is_timeout() const noexcept {
	return code == CURLE_OPERATION_TIMEDOUT;
}

bool co_curl::result::is_range_error() const noexcept {
	return code == CURLE_RANGE_ERROR;
}

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

auto co_curl::easy_handle::sync_perform() noexcept -> result {
	// TODO do something with return code
	return result{.code = curl_easy_perform(native_handle)};
}

auto co_curl::easy_handle::perform() noexcept -> co_curl::perform {
	return {*this};
}

void co_curl::easy_handle::url(const char * u) {
	curl_easy_setopt(native_handle, CURLOPT_URL, u);
}

auto co_curl::easy_handle::url() const noexcept -> std::string_view {
	char * ptr = nullptr;
	curl_easy_getinfo(native_handle, CURLINFO_EFFECTIVE_URL, &ptr);
	assert(ptr != nullptr);
	return std::string_view{ptr};
}

void co_curl::easy_handle::follow_location(bool enable) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_FOLLOWLOCATION, static_cast<long>(enable));
}

void co_curl::easy_handle::verbose(bool enable) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_VERBOSE, static_cast<long>(enable));
}

#ifndef LIBCURL_BEFORE_NEEDED
void co_curl::easy_handle::pipewait(bool enable) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_PIPEWAIT, static_cast<long>(enable));
}
#endif

void co_curl::easy_handle::fresh_connect(bool enable) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_FRESH_CONNECT, static_cast<long>(enable));
}

void co_curl::easy_handle::upload(bool enable) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_UPLOAD, static_cast<long>(enable));
}

void co_curl::easy_handle::infile_size(size_t size) noexcept {
	// static_assert(size_of(curl_off_t) == size_of(size_t));
	curl_easy_setopt(native_handle, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(size));
}

void co_curl::easy_handle::prequote(list & lst) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_PREQUOTE, lst.data);
}

void co_curl::easy_handle::quote(list & lst) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_QUOTE, lst.data);
}

void co_curl::easy_handle::postquote(list & lst) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_POSTQUOTE, lst.data);
}

void co_curl::easy_handle::http_headers(list & lst) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_HTTPHEADER, lst.data);
}

void co_curl::easy_handle::username(const char * in) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_USERNAME, in);
}

void co_curl::easy_handle::password(const char * in) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_PASSWORD, in);
}

void co_curl::easy_handle::resume(size_t position) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(position));
}

void co_curl::easy_handle::disable_resume() noexcept {
	resume(0u);
}

void co_curl::easy_handle::connection_timeout(std::chrono::milliseconds duration) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(duration.count()));
}

void co_curl::easy_handle::low_speed_timeout(std::chrono::seconds duration, size_t bytes_per_second) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_LOW_SPEED_LIMIT, static_cast<long>(bytes_per_second));
	curl_easy_setopt(native_handle, CURLOPT_LOW_SPEED_TIME, static_cast<long>(duration.count()));
}

void co_curl::easy_handle::low_speed_timeout(size_t bytes_per_second, std::chrono::seconds duration) noexcept {
	low_speed_timeout(duration, bytes_per_second);
}

void co_curl::easy_handle::set_coroutine_handle(std::coroutine_handle<void> h) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_PRIVATE, h.address());
}

auto co_curl::easy_handle::get_coroutine_handle() noexcept -> std::coroutine_handle<void> {
	void * ptr = nullptr;
	curl_easy_getinfo(native_handle, CURLINFO_PRIVATE, &ptr);
	return std::coroutine_handle<void>::from_address(ptr);
}

void co_curl::easy_handle::ssl_verify_peer(bool enable) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_SSL_VERIFYPEER, static_cast<long>(enable));
}

void co_curl::easy_handle::write_function(size_t (*f)(char *, size_t, size_t, void *)) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_WRITEFUNCTION, f);
}

void co_curl::easy_handle::write_data(void * udata) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_WRITEDATA, udata);
}

void co_curl::easy_handle::read_function(size_t (*f)(char *, size_t, size_t, void *)) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_READFUNCTION, f);
}

void co_curl::easy_handle::read_data(void * udata) noexcept {
	curl_easy_setopt(native_handle, CURLOPT_READDATA, udata);
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

#ifndef LIBCURL_BEFORE_NEEDED
auto co_curl::easy_handle::get_content_length() const noexcept -> std::optional<size_t> {
	curl_off_t cl{0};

	if (CURLE_OK != curl_easy_getinfo(native_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl)) {
		return std::nullopt;
	}

	return static_cast<size_t>(cl);
}
#endif

auto co_curl::easy_handle::get_response_code() const noexcept -> unsigned {
	long rc{0};

	curl_easy_getinfo(native_handle, CURLINFO_RESPONSE_CODE, &rc);

	return static_cast<unsigned>(rc);
}
