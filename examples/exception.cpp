#include <co_curl/co_curl.hpp>

auto fetch(std::string url) -> co_curl::promise<std::string> {
	auto handle = co_curl::easy_handle{url};

	std::string output;

	handle.write_into(output);

	auto r = co_await handle.perform();

	if (!r) {
		throw std::runtime_error{"can't download!"};
	}

	co_return output;
}

auto outer(std::string url) -> co_curl::promise<std::string> {
	// test propagation of exception
	try {
		co_return co_await fetch(url);
	} catch (const std::runtime_error & re) {
		co_return "some exception somewhere";
	}
}

int main() {
	try {
		std::string a = fetch("https://nonexisting-domain");
		std::cout << a << "\n";
	} catch (const std::runtime_error & re) {
		std::cerr << re.what() << "\n";
	}

	try {
		std::string a = outer("https://nonexisting-domain");
		std::cout << a << "\n";
	} catch (const std::runtime_error & re) {
		std::cerr << re.what() << "\n";
	}
}