#include "co_curl.hpp"
#include <curl/curl.h>

auto co_curl::curl_version() noexcept -> std::string_view {
	return ::curl_version();
}
