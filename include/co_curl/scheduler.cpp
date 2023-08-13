#include "scheduler.hpp"
#include <curl/curl.h>

static auto get_coroutine_handle(CURL * handle) noexcept -> std::coroutine_handle<void> {
	void * ptr = nullptr;
	curl_easy_getinfo(handle, CURLINFO_PRIVATE, &ptr);
	return std::coroutine_handle<void>::from_address(ptr);
}

auto co_curl::waiting_coroutines_for_curl_finished::trigger(CURL * handle) noexcept -> std::coroutine_handle<void> {
	auto next_coro = get_coroutine_handle(handle);
	curl.remove_handle(handle);
	return next_coro;
}