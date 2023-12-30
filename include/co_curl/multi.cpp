#include "multi.hpp"
#include <exception>
#include <limits>
#include <curl/curl.h>

co_curl::multi_handle::multi_handle(): native_handle{curl_multi_init()} { }

co_curl::multi_handle::~multi_handle() noexcept {
	if (native_handle) {
		curl_multi_cleanup(native_handle);
	}
}

void co_curl::multi_handle::add_handle(easy_handle & handle) {
	curl_multi_add_handle(native_handle, handle.native_handle);
}

bool co_curl::multi_handle::remove_handle(easy_handle & handle) noexcept {
	return remove_handle(handle.native_handle);
}

bool co_curl::multi_handle::remove_handle(CURL * handle) noexcept {
	return CURLM_OK == curl_multi_remove_handle(native_handle, handle);
}

bool co_curl::multi_handle::sync_perform(unsigned & running_handles) {
	int tmp = 0;

	if (CURLM_OK != curl_multi_perform(native_handle, &tmp)) {
		return false;
	}

	assert(tmp <= (std::numeric_limits<int>::max)());
	assert(tmp >= 0);

	running_handles = static_cast<unsigned>(tmp);
	return true;
}

auto co_curl::multi_handle::sync_perform() -> std::optional<unsigned> {
	unsigned running_handles = 0;

	if (!sync_perform(running_handles)) {
		return std::nullopt;
	}

	return running_handles;
}

bool co_curl::multi_handle::poll(std::chrono::milliseconds timeout) noexcept {
#ifndef LIBCURL_BEFORE_NEEDED
	return CURLM_OK == curl_multi_poll(native_handle, nullptr, 0, static_cast<int>(timeout.count()), nullptr);
#else
	return CURLM_OK == curl_multi_wait(native_handle, nullptr, 0, static_cast<int>(timeout.count()), nullptr);
#endif
}

auto co_curl::multi_handle::info_read(unsigned & msg_remaining) noexcept -> std::optional<message> {
	int msg_count{0};
	CURLMsg * msg = curl_multi_info_read(native_handle, &msg_count);

	assert(msg_count <= (std::numeric_limits<int>::max)());
	assert(msg_count >= 0);

	msg_remaining = static_cast<unsigned>(msg_count);

	if (!msg) {
		return std::nullopt;
	}

	if (msg->msg == CURLMSG_DONE) {
		return message{finished{msg->easy_handle, msg->data.result}};
	}

	std::terminate();
}

auto co_curl::multi_handle::get_finished() -> std::optional<finished> {
	int msg_count{0};
	CURLMsg * msg = curl_multi_info_read(native_handle, &msg_count);

	assert(msg_count <= (std::numeric_limits<int>::max)());
	assert(msg_count >= 0);

	// msg_remaining = static_cast<unsigned>(msg_count);

	if (!msg) {
		return std::nullopt;
	}

	if (msg->msg == CURLMSG_DONE) {
		return finished{msg->easy_handle, msg->data.result};
	}

	std::terminate();
}

void co_curl::multi_handle::max_total_connections(unsigned number) noexcept {
	curl_multi_setopt(native_handle, CURLMOPT_MAX_TOTAL_CONNECTIONS, (long)number);
}
