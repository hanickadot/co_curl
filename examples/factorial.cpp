#include <co_curl/co_curl.hpp>

auto factorial(uint64_t i) -> co_curl::task<uint64_t> {
	if (i == 0) {
		co_return 1u;
	} else {
		uint64_t v = co_await factorial(i - 1u);
		uint64_t r = i * v;
		std::cout << r << "\n";
		co_return r;
	}
}

int main() {
	factorial(15).get();
}