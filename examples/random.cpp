#include <co_curl/co_curl.hpp>
#include <exception>

auto fetch(std::string url, unsigned id, unsigned & id_out) -> co_curl::task<void> {
	auto handle = co_curl::easy_handle{url};

	handle.fresh_connect();

	std::string output;

	handle.write_into(output);

	if (!co_await handle.perform()) {
		throw std::runtime_error{"unable to download requested document"};
	}

	if (!id_out) {
		id_out = id;
	}
}

auto select_first() -> co_curl::task<unsigned> {
	unsigned result = 0;

	auto a = fetch("https://hanicka.net/a.txt", 1, result);
	auto b = fetch("https://hanicka.net/b.txt", 2, result);

	// TODO:
	// auto r = co_await co_curl::first({a,b});
	// and
	// auto r = co_await co_curl::all({a,b});

	co_await a;
	co_await b;

	co_return result;
}

int main() {
	std::cout << select_first() << "\n";
}