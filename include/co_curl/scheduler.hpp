#ifndef CO_CURL_SCHEDULER_HPP
#define CO_CURL_SCHEDULER_HPP

#include <iostream>
#include <queue>
#include <cassert>
#include <coroutine>

namespace co_curl {

struct default_scheduler {
	// all resuming is going to happen thru a coroutine_handle return
	std::queue<std::coroutine_handle<>> ready_coroutines{};

	auto get_first_ready_coroutine() -> std::coroutine_handle<> {
		const auto first = ready_coroutines.front();
		ready_coroutines.pop();
		return first;
	}

	auto run_background_actions_and_find_next_coroutine_to_run() -> std::coroutine_handle<> {
		// TODO implement it here
		return std::noop_coroutine();
	}

	auto coroutine_awaiting() -> std::coroutine_handle<> {
		// look for anything to run and run it (usually case of multiple awaiters)
		std::cerr << "<coroutine_awaiting>\n";

		if (!ready_coroutines.empty()) {
			return get_first_ready_coroutine();
		}

		return run_background_actions_and_find_next_coroutine_to_run();
	}
	auto coroutine_finished(std::coroutine_handle<> awaiter = {}) -> std::coroutine_handle<> {
		std::cerr << "<coroutine_finished>\n";

		if (awaiter) {
			// TODO if there is more awaiters, put second and rest to ready_coroutines
			return awaiter;
		}

		// if there is no awaiter, look for coroutines which are ready...
		if (!ready_coroutines.empty()) {
			return get_first_ready_coroutine();
		}

		// ... or look for something else to finish ...
		return run_background_actions_and_find_next_coroutine_to_run();
	}
	void set_additional_awaiters(std::coroutine_handle<> awaitee, std::coroutine_handle<> awaiter) {
		(void)awaitee;
		(void)awaiter;
		assert(false);
	}
};

template <typename T> auto get_scheduler() -> T & {
	static T global_scheduler{};
	return global_scheduler;
}

} // namespace co_curl

#endif
