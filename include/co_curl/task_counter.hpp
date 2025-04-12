#ifndef CO_CURL_TASK_COUNTER_HPP
#define CO_CURL_TASK_COUNTER_HPP

namespace co_curl {

#ifdef CO_CURL_DEBUG

struct task_counter {
	unsigned tasks_in_running{0};
	unsigned tasks_blocked{0};
	unsigned tasks_in_different_thread{0};

	constexpr void start() noexcept {
		++tasks_in_running;
	}

	constexpr void finish() noexcept {
		--tasks_in_running;
	}

	constexpr void switched_to_different_thread() noexcept {
		++tasks_in_different_thread;
	}

	constexpr void switched_back_to_main_thread() noexcept {
		--tasks_in_different_thread;
	}

	constexpr void blocked() noexcept {
		++tasks_blocked;
	}

	constexpr void unblocked() noexcept {
		--tasks_blocked;
	}

	constexpr bool graph_blocked() const noexcept {
		return (tasks_in_running - tasks_in_different_thread) == tasks_blocked;
	}

	inline void print() const noexcept {
		std::cout << "tasks: " << tasks_in_running;
		std::cout << ", sleeping: " << tasks_blocked;
		std::cout << ", different thread: " << tasks_in_different_thread;

		if (graph_blocked()) {
			std::cout << " [blocked]";
		}

		std::cout << "\n";
	}
};

#else

struct task_counter {
	int blocked_balance{0};

	constexpr void start() noexcept {
		++blocked_balance;
	}

	constexpr void finish() noexcept {
		--blocked_balance;
	}

	constexpr void switched_to_different_thread() noexcept {
		--blocked_balance;
	}

	constexpr void switched_back_to_main_thread() noexcept {
		++blocked_balance;
	}

	constexpr void blocked() noexcept {
		--blocked_balance;
	}

	constexpr void unblocked() noexcept {
		++blocked_balance;
	}

	constexpr bool graph_blocked() const noexcept {
		return blocked_balance == 0;
	}

	inline void print() const noexcept {
		std::cout << "balance: " << blocked_balance;

		if (graph_blocked()) {
			std::cout << " [blocked]";
		}

		std::cout << "\n";
	}
};

#endif

} // namespace co_curl

#endif
