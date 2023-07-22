#include <co_curl/co_curl.hpp>
#include <iostream>

auto fetch(std::string url) -> co_curl::task<std::string> {
	auto handle = co_curl::easy_handle{url};

	std::string output;

	handle.write_into(output);
	handle.pipewait();

	std::cout << "| downloading " << url << "...\n";

	if (!co_await co_curl::perform(handle)) {
		co_return "";
	}

	std::cout << "| " << url << " downloaded (size = " << output.size();

	std::cout << ", response-code: " << handle.get_response_code();

	if (auto mime_type = handle.get_content_type()) {
		std::cout << ", mime-type: " << *mime_type;
	}

	std::cout << ")\n";
	co_return output;
}

auto download_index() -> co_curl::task<void> {
	co_await fetch("https://hanicka.net/FPL07048.jpg");
}

auto download_left(const co_curl::task<void> & index) -> co_curl::task<std::string> {
	std::cout << "download_left...\n";
	co_await index;
	auto r = std::string("left");
	std::cout << "left: r.size() = " << r.size() << "\n";
	co_return std::move(r);
}

auto download_right(const co_curl::task<void> & index) -> co_curl::task<std::string> {
	std::cout << "download_right...\n";
	co_await index;
	auto r = std::string("right");
	std::cout << "right: r.size() = " << r.size() << "\n";
	co_return std::move(r);
}

auto test() -> co_curl::task<std::string> {
	const auto index = download_index();

	auto a = download_left(index);
	auto b = download_right(index);

	co_return co_await a + co_await b;
}

int main() {
	const std::string r = test();
	std::cout << "total size: " << r.size() << "\n";
}
