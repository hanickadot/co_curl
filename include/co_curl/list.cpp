#include "list.hpp"
#include <exception>
#include <curl/curl.h>

// accessing internals of the CURL library (don't do that)

// extern "C" struct curl_slist * Curl_slist_append_nodup(struct curl_slist * list, char * data);

co_curl::list::~list() noexcept {
	if (data) {
		curl_slist_free_all(data);
	}
}

void co_curl::list::append(const char * str) {
	if (auto * r = curl_slist_append(data, str)) {
		data = r;
	} else {
		throw std::runtime_error("curl_slist_append() failed");
	}
}

struct c_free_deleter {
	void operator()(char * ptr) noexcept {
		if (ptr) {
			free(reinterpret_cast<void *>(ptr));
		}
	}
};

/*
void co_curl::list::append(std::string_view in) {
	auto ptr = std::unique_ptr<char, c_free_deleter>(strndup(in.data(), in.size()));

	if (!ptr) {
		throw std::runtime_error("list::append(std::string_view)::dup() failed");
	}

	if (auto * r = Curl_slist_append_nodup(data, ptr.get())) {
		data = r;
		ptr.release();
	} else {
		throw std::runtime_error("Curl_slist_append_nodup() failed");
	}
}
*/
