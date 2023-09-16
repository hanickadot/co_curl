#ifndef CO_CURL_FETCH_HPP
#define CO_CURL_FETCH_HPP

#include "easy.hpp"

namespace co_curl {

template <typename Container = std::string> auto fetch(std::string_view url, int attempts = 5) -> co_curl::promise<Container> {
	auto handle = co_curl::easy_handle{url};

	Container output;

	handle.verbose();
	handle.follow_location();
	handle.write_into(output);
	handle.connection_timeout(std::chrono::seconds{2});
	handle.low_speed_timeout(100, std::chrono::seconds{1});

	for (;;) {
		auto r = co_await handle.perform();

		if (!r) {
			if (r.is_range_error()) {
				output.clear();
				handle.disable_resume();
				continue;
			}

			if (attempts-- > 0) {
				handle.resume(output.size());
				continue;
			}

			throw std::runtime_error(std::string{"couldn't download a file: "}.append(handle.url()) + " (reason: " + r.c_str() + ")");
		}

		// if (handle.get_response_code() != co_curl::http_2XX) {
		//	throw std::runtime_error(std::string{"url doesn't return HTTP 2xx response: "}.append(handle.url()));
		// }

		co_return output;
	}
}

} // namespace co_curl

#endif
