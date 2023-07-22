#include <co_curl/co_curl.hpp>

auto fetch(std::string url) -> co_curl::task<std::string> {
	auto handle = co_curl::easy_handle{url};

	std::string output;

	handle.write_into(output);

	if (!co_await handle.perform()) {
		co_return "weird";
	}

	co_return output;
}

auto recursion(co_curl::task<std::string> task, std::string url, unsigned i = 0) -> co_curl::task<std::string> {
	if (i < 1000) {
		std::cout << i << " ";
		co_return co_await recursion(std::move(task), url, i + 1);
	}

	co_return co_await task + " " + co_await fetch(url) + "";
}

auto start() -> co_curl::task<std::string> {
	// hello + world
	co_return co_await recursion(fetch("https://hanicka.net/a.txt"), "https://hanicka.net/b.txt");
}

int main() {
	std::cout << start().get() << "\n";
}