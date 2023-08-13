#include <co_curl/co_curl.hpp>
#include <co_curl/format.hpp>
#include <iostream>

auto fetch(std::string url) -> co_curl::promise<std::string> {
	auto handle = co_curl::easy_handle{url};

	std::string output;

	handle.write_into(output);
	handle.pipewait();

	std::cout << "| downloading " << url << "...\n";

	if (!co_await handle.perform()) {
		co_return "";
	}

	std::cout << "| " << url << " downloaded (size = " << co_curl::data_amount(output.size());

	std::cout << ", response-code: " << handle.get_response_code();

	if (auto mime_type = handle.get_content_type()) {
		std::cout << ", mime-type: " << *mime_type;
	}

	std::cout << ")\n";
	co_return output;
}

auto download_two() -> co_curl::promise<std::string> {
	std::cout << "1 download_two: start\n";

	auto a = fetch("https://hanicka.net/");

	std::cout << "1 download_two: after first\n";

	auto b = fetch("https://compile-time.re/");

	std::cout << "1 download_two: after second\n";

	auto r = co_await a + co_await b;

	std::cout << "1 download_two: after finish\n";

	co_return r;
}

auto download_two_b() -> co_curl::promise<std::string> {
	std::cout << "2 download_two: start\n";

	auto a = fetch("https://hanickadot.github.io/");

	std::cout << "2 download_two: after first\n";

	auto b = fetch("https://talks.cpp.fail/cppnow-2023-lightning-updates/");

	std::cout << "2 download_two: after second\n";

	auto r = co_await a + co_await b;

	std::cout << "2 download_two: after finish\n";

	co_return r;
}

auto download_four() -> co_curl::promise<std::string> {
	auto a = download_two();
	auto b = download_two_b();

	std::cout << "\n================\n\n";

	co_return co_await a + co_await b;
}

int main() {
	const std::string r = download_four();
	std::cout << "total downloaded amount: " << co_curl::data_amount(r.size()) << "\n";
}
