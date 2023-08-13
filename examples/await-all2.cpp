#include <co_curl/all.hpp>
#include <list>

auto fetch(std::string url) -> co_curl::promise<std::optional<std::string>> {
	auto handle = co_curl::easy_handle{url};

	std::string output;

	// handle.verbose();
	handle.write_into(output);

	std::cout << "downloading of: '" << url << "'\n";

	auto r = co_await handle.perform();

	if (!r) {
		co_return std::nullopt;
	}

	std::cout << "'" << url << "' downloaded successfully\n";

	co_return output;
}

auto main_coroutine() -> co_curl::promise<int> {
	std::cout << "-- curl will start here --\n";

	auto result = co_await co_curl::all(fetch("https://hanicka.net/a.txt"), fetch("https://hanicka.net/b.txt"));

	std::cout << "-- curl is finished here --\n";

	if (!result) {
		std::cout << "something wasn't downloaded!";
		co_return 1;
	}

	auto && [a, b] = *result;

	std::cout << a << b << "\n";

	co_return 0;
}

int main() {
	return main_coroutine();
}