#include "co_curl.hpp"
#include <curl/curl.h>

void co_curl::global_init() noexcept {
	curl_global_init(CURL_GLOBAL_ALL);
}
