#include <co_curl/co_curl.hpp>

auto fetch(std::string url) -> co_curl::task<bool> {
	auto handle = co_curl::easy_handle{url};

	handle.write_nowhere();

	co_return bool(co_await handle.perform());
}

int main() {
	bool a = fetch("https://nonexisting-domain");
	bool b = fetch("https://www.google.com");

	if (a == false) {
		std::cout << "first fetch failed! (as expected)\n";
	} else {
		return 1;
	}

	if (b == true) {
		std::cout << "second fetch succeed! (as expected)\n";
	} else {
		return 1;
	}
}