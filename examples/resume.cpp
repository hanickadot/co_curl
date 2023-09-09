#include <co_curl/co_curl.hpp>
#include <string>

auto fetch(std::string url, std::optional<unsigned> maximum_number_of_retries = std::nullopt) -> co_curl::promise<std::vector<std::byte>> {
	auto handle = co_curl::easy_handle(url);

	auto result = std::vector<std::byte>{};
	handle.write_into(result);

	// it will timeout when receive less than 100B per 10s
	handle.low_speed_timeout(std::chrono::seconds{10}, 100);

	for (unsigned i = 0; i < maximum_number_of_retries.value_or(~0u); ++i) {
		const auto r = co_await handle.perform();

		// repeat if we didn't finish properly (timeout / transfer error)
		if (!r) {
			handle.resume(result.size());
			continue;
		}

		if (handle.get_response_code() != co_curl::http_2XX) {
			throw std::runtime_error{"non 2XX HTTP response code"};
		}

		co_return result;
	}

	throw std::runtime_error{"maximum number of retries reached"};
}

int main() {
	auto result = fetch("http://somewhere/large.bin");
}
