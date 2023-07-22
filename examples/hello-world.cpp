#include <co_curl/co_curl.hpp>
#include <exception>

auto fetch(std::string url) -> co_curl::task<std::string> {
	auto handle = co_curl::easy_handle{url};

	std::string output;

	handle.write_into(output);

	if (!co_await handle.perform()) {
		throw std::runtime_error{"unable to download requested document"};
	}

	co_return output;
}

int main() {
	std::cout << fetch("https://hanicka.net") << "\n";
}