#include <co_curl/co_curl.hpp>

auto show_content(std::string url) -> co_curl::task<void> {
	co_await co_curl::easy_handle{url}.perform();
}

int main() {
	show_content("https://hanicka.net/");
}