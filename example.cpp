#include <co_curl/co_curl.hpp>
#include <iostream>

auto fetch(std::string url) -> co_curl::task<std::string> {
	auto handle = co_curl::easy_handle{url};

	std::string output;

	auto write = [&](std::string_view data) {
		output += data;
	};

	// handle.set_verbose();
	handle.set_write_callback(write);

	std::cout << "| downloading " << url << "...\n";

	if (!co_await co_curl::perform(handle)) {
		co_return "";
	}

	std::cout << "| " << url << " downloaded (size = " << output.size() << ")\n";
	co_return output;
}

auto download_two() -> co_curl::task<std::string> {
	std::cout << "1 download_two: start\n";

	auto a = fetch("https://hanicka.net/");

	std::cout << "1 download_two: after first\n";

	auto b = fetch("https://nimue.cz/");

	std::cout << "1 download_two: after second\n";

	auto r = co_await a + co_await b;

	std::cout << "1 download_two: after finish\n";

	co_return r;
}

auto download_two_b() -> co_curl::task<std::string> {
	std::cout << "2 download_two: start\n";

	auto a = fetch("https://hanickadot.github.io/");

	std::cout << "2 download_two: after first\n";

	auto b = fetch("https://talks.cpp.fail/");

	std::cout << "2 download_two: after second\n";

	auto r = co_await a + co_await b;

	std::cout << "2 download_two: after finish\n";

	co_return r;
}

auto download_four() -> co_curl::task<std::string> {
	auto a = download_two();
	auto b = download_two_b();
	co_return co_await a + co_await b;
}

int main() {
	const std::string r = download_four();
	std::cout << r.size() << "\n";
}
