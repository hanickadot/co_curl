#include <co_curl/co_curl.hpp>
#include <iostream>

auto fetch(std::string url) -> co_curl::task<std::string> {
	auto handle = co_curl::easy_handle{url};

	std::string output;

	auto write = [&](std::string_view data) {
		std::cout << "| incoming data: " << data.size() << "\n";
		output += data;
	};

	// handle.set_verbose();
	handle.set_write_callback(write);

	std::cout << "| downloading " << url << "...\n";

	if (!co_await co_curl::perform(handle)) {
		co_return "";
	}

	std::cout << "| " << url << " downloaded\n";

	co_return output;
}

auto download_two() -> co_curl::task<std::string> {
	std::cout << "download_two: start\n";

	auto a = fetch("https://hanicka.net/");

	std::cout << "download_two: after first\n";

	auto b = fetch("https://www.google.com/");

	std::cout << "download_two: after second\n";

	auto r = co_await a + co_await b;

	std::cout << "download_two: after finish\n";

	co_return r;
}

int main() {
	const std::string r = download_two();
	std::cout << r.size() << "\n";
}
