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

auto process_left(const co_curl::task<void> & index) -> co_curl::task<size_t> {
	std::cout << "download_left...\n";
	co_await index;

	co_return 1;
}

auto process_right(const co_curl::task<void> & index) -> co_curl::task<size_t> {
	std::cout << "download_right...\n";
	co_await index;

	co_return 2;
}

auto test() -> co_curl::task<void> {
	const auto index = download_index();

	std::cout << "before processing...\n";

	const auto a = process_left(index);
	const auto b = process_right(index);

	const auto size = co_await a + co_await b;
	std::cout << "total size: " << size << "\n";
}

int main() {
	test();
}