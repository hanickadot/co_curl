#include <co_curl/thread-pool.hpp>
#include <iostream>

auto test2(co_curl::thread_pool &) -> co_curl::detached_task<int> {
	constexpr auto wait = std::chrono::seconds{1};
	std::cout << "test2: wait for " << wait.count() << " seconds.\n";
	std::this_thread::sleep_for(wait);
	std::cout << "test2: wait done.\n";
	co_return 5;
}

auto test3(co_curl::thread_pool &) -> co_curl::detached_task<int> {
	constexpr auto wait = std::chrono::seconds{1};
	std::cout << "test3: wait for " << wait.count() << " seconds.\n";
	std::this_thread::sleep_for(wait);
	std::cout << "test3: wait done.\n";
	co_return 7;
}

auto test(co_curl::thread_pool & tp) -> co_curl::detached_task<int> {
	constexpr auto wait = std::chrono::seconds{2};

	auto t2 = test2(tp);
	auto t3 = test3(tp);

	std::cout << "test1: wait for " << wait.count() << " seconds.\n";
	std::this_thread::sleep_for(wait);
	std::cout << "test1: wait done.\n";

	auto a = co_await t2;

	std::cout << "test1: between co_await\n";

	auto b = co_await t3;

	std::cout << "test1: after co_await\n";

	co_return a * b;
}

int main() {
	std::cout << "main()\n";
	auto tp = co_curl::thread_pool{1};

	auto r = co_curl::sync_await(test(tp));
	std::cout << "when job is done...\n";

	tp.stop_and_join();

	std::cout << "result = " << r << "\n";

	std::cout << "~main()\n";
}