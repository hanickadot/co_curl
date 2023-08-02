#include <co_curl/co_curl.hpp>
#include <exception>

struct move_only_string: std::string {
	using super = std::string;
	move_only_string() noexcept: super{} { }
	move_only_string(move_only_string && other) noexcept: super{std::move(other)} {};

	move_only_string & operator=(move_only_string && other) noexcept {
		super::operator=(std::move(other));
		return *this;
	}

	move_only_string & operator=(const move_only_string & other) noexcept = delete;
};

auto fetch(std::string url) -> co_curl::task<move_only_string> {
	auto handle = co_curl::easy_handle{url};

	move_only_string output{};

	handle.write_into(output);

	if (!co_await handle.perform()) {
		throw std::runtime_error{"unable to download requested document"};
	}

	co_return output;
}

auto example() -> co_curl::task<move_only_string> {
	co_return co_await fetch("https://hanicka.net");
}

int main() {
	std::cout << fetch("https://hanicka.net") << "\n";
}