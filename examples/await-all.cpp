#include <co_curl/all.hpp>
#include <list>

auto fetch(std::string url) -> co_curl::task<std::optional<std::string>> {
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

auto main_coroutine(const std::initializer_list<std::string> & urls) -> co_curl::task<int> {
	auto download = [](std::string url) { return fetch(url); };

	std::cout << "-- curl will start here --\n";

	auto r = co_await co_curl::all(urls | std::views::transform(download));

	std::cout << "-- curl is finished here --\n";

	if (!r) {
		std::cout << "something wasn't downloaded!";
		co_return 1;
	}

	for (const std::string & item: *r) {
		std::cout << item << "\n";
	}

	co_return 0;
}

int main() {
	return main_coroutine({
		// "https://non-existing-domain/",
		"https://hanicka.net/a.txt",
		"https://hanicka.net/b.txt",
	});
}