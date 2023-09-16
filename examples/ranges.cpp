#include <co_curl/all.hpp>
#include <co_curl/co_curl.hpp>
#include <co_curl/fetch.hpp>
#include <ctre.hpp>
#include <iostream>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>

template <typename T> struct ranges_to;

template <typename T> struct ranges_to<std::set<T>> {
	friend auto operator|(auto && range, ranges_to) {
		std::set<T> output{};
		for (auto x: range) {
			output.emplace(x);
		}
		return output;
	}
};

struct file {
	std::string url;
	std::string content;
};

auto fetch_with_name(std::string_view url) -> co_curl::promise<file> {
	co_return file{.url = std::string(url), .content = co_await co_curl::fetch(url)};
}

auto download_all(std::string_view url) -> co_curl::promise<std::vector<file>> {
	co_return co_await (
		co_await co_curl::fetch(url)			 // download index
		| ctre::range<R"(https?://[^"'\s)]++)">	 // extract all absolute URLs
		| ranges_to<std::set<std::string>>()	 // replacement for std::ranges::to<std::set<std::string>>
		| std::views::transform(fetch_with_name) // download them
		| co_curl::all);						 // wait for all to complete
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
