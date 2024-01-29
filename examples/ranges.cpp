#include <co_curl/all.hpp>
#include <co_curl/co_curl.hpp>
#include <co_curl/fetch.hpp>
#include <ctre.hpp>
#include <iostream>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>

struct file {
	std::string url;
	std::string content;
};

auto fetch_with_name(std::string_view url) -> co_curl::promise<file> {
	co_return file{.url = std::string(url), .content = co_await co_curl::fetch(url)};
}

auto download_all(std::string_view url) -> co_curl::promise<std::vector<file>> {
	co_return co_await (
		co_await co_curl::fetch(url)				 // download index
		| ctre::search_all<R"(https?://[^"'\s)]++)"> // extract all absolute URLs
		| std::views::transform(fetch_with_name)	 // download them
		| co_curl::all);							 // wait for all to complete
}

int main(int argc, char ** argv) {
	if (argc != 2) {
		std::cerr << "usage: ranges URL\n";
		return 1;
	}

	std::vector<file> result = download_all(argv[1]); // converting to a concrete type is equivalent of calling .get()

	std::cout << "count = " << result.size() << "\n";
	for (const auto & [url, content]: result) {
		std::cout << " url = " << url << ", ";
		std::cout << " content size = " << content.size() << "\n";
	}
}
