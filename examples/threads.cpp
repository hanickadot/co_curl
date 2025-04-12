#include <co_curl/all.hpp>
#include <co_curl/co_curl.hpp>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <ranges>
#include <string>
#include <syncstream>

struct osyncstream {
	static auto get_lock() {
		static std::mutex mtx{};
		return std::unique_lock(mtx);
	}

	std::unique_lock<std::mutex> lock;
	std::ostream & stream;

	osyncstream(std::ostream & os): lock{get_lock()}, stream{os} { }

	friend osyncstream & operator<<(osyncstream & sos, auto && anything) {
		return ((sos.stream << std::forward<decltype(anything)>(anything)), sos);
	}

	friend osyncstream & operator<<(osyncstream && sos, auto && anything) {
		return ((sos.stream << std::forward<decltype(anything)>(anything)), sos);
	}
};

void heavy_cpu_task(std::chrono::milliseconds ms) {
	auto start = std::chrono::steady_clock::now();
	while (start + ms >= std::chrono::steady_clock::now()) {
		// activelly loop for 5 seconds
	}
}

auto thread_test(size_t id) -> co_curl::promise<int> {
	osyncstream(std::cout) << std::this_thread::get_id() << ": #" << id << ": start task (still in main thread)\n";

	// switch for heavy thread task to other thread
	co_await co_curl::move_to_new_thread;

	osyncstream(std::cout) << " " << std::this_thread::get_id() << ": #" << id << ": different thread\n";

	heavy_cpu_task(std::chrono::seconds{1});

	if (id % 2 != 0) {
		// we can return back to main thread easily
		co_await co_curl::move_back_to_main_thread;
		osyncstream(std::cout) << " " << std::this_thread::get_id() << ": #" << id << ": back in main thread\n";
	}

	co_return 1;
}

auto spawn_threads_and_sum_result(size_t count) -> co_curl::promise<int> {
	auto tasks = std::vector<co_curl::promise<int>>();

	for (size_t i = 0; i != count; ++i) {
		tasks.emplace_back(thread_test(i));
	}

	std::vector<int> results = co_await co_curl::all(tasks);
	std::cout << std::this_thread::get_id() << ": all synced\n";

	co_return std::reduce(results.begin(), results.end(), 0);
}

int main() {
	std::cout << "sum = " << spawn_threads_and_sum_result(14) << " (must return 14)\n";
}
