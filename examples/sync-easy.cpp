#include <co_curl/co_curl.hpp>

int main() {
	auto handle = co_curl::easy_handle{"https://hanicka.net/"};

	std::string output;

	handle.write_into(output);

	if (!handle.perform()) {
		std::cout << "error\n";
		return 1;
	}

	std::cout << output << "\n";
	return 0;
}