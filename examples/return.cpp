#include "/Volumes/projekty/tmp6/compile-time-regular-expressions/include/ctre.hpp"
#include <co_curl/all.hpp>
#include <co_curl/co_curl.hpp>
#include <ranges>

struct no_attempts_left {
	std::string url;
};
struct url_not_available {
	std::string url;
};

auto fetch(std::string url, int attempts = 10) -> co_curl::promise<std::string> {
	std::string output{};

	auto handle = co_curl::easy_handle{url};
	handle.write_into(output);
	handle.follow_location();

	// try multiple times...
	for (;;) {
		if (attempts-- <= 0) {
			throw no_attempts_left(url);
		}

		if (!co_await handle.perform()) {
			handle.resume(output.size()); // keep what was downloaded
			continue;					  // try again
		}

		if (handle.get_response_code() != co_curl::http_2XX) {
			throw url_not_available(url);
		}

		co_return output;
	}
}

struct name_and_content {
	std::string url;
	std::string content;
};

auto fetch_with_name(std::string_view url) -> co_curl::promise<name_and_content> {
	std::cout << url << "\n";
	co_return name_and_content{.url = std::string(url), .content = co_await fetch(std::string(url))};
}

auto fetch_all(std::string url) -> co_curl::promise<std::vector<name_and_content>> {
	const auto index = co_await fetch(url);
	const auto links = ctre::range<R"(https?://[^"'\s]++)">(index);

	// it will download all in parallel (in one thread :)
	co_return co_await co_curl::all(links | std::views::transform(fetch_with_name));
}

int main() {
	fetch_all("https://www.google.com").get();
}