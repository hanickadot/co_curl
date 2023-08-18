#include <co_curl/select.hpp>
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

template <typename> struct identify;

auto main_coroutine() -> co_curl::promise<int> {
	std::cout << "-- curl will start here --\n";

	// get first to complete..., rest will be destroyed
	auto result = co_await co_curl::select(fetch("https://hanicka.net/a.txt"), fetch("https://hanicka.net/b.txt"));

	std::cout << "-- curl is finished here --\n";

	if (!result) {
		std::cout << "something wasn't downloaded!";
		co_return 1;
	}

	// identify<decltype(result)> i;

	std::cout << *result << "\n";

	co_return 0;
}

int main() {
	return main_coroutine();
}