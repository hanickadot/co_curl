#ifndef CO_CURL_THREAD_POOL_HPP
#define CO_CURL_THREAD_POOL_HPP

#include <deque>
#include <functional>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>
#include <coroutine>

namespace co_curl {

struct thread_pool {
	std::deque<std::function<void(void)>> queue{};
	std::vector<std::thread> threads{};
	std::mutex mutex{};
	std::condition_variable cv{};

	struct stop_exception {
	};

	explicit thread_pool(size_t count) {
		add_threads(count);
	}

	thread_pool(const thread_pool &) = delete;
	thread_pool(thread_pool &&) = delete;

	~thread_pool() noexcept {
		stop_and_join();
	}

	void stop_and_join() {
		stop_all_threads();
		for (std::thread & th: threads) {
			th.join();
		}
		threads.resize(0);
	}

	auto obtain_task_from_bottom() {
		auto lock = std::unique_lock(mutex);
		cv.wait(lock, [this] { return !queue.empty(); });

		auto res = std::move(queue.front());
		queue.pop_front();
		return res;
	}

	auto obtain_task_from_top() {
		auto lock = std::unique_lock(mutex);
		cv.wait(lock, [this] { return !queue.empty(); });

		auto res = std::move(queue.front());
		queue.pop_back();
		return res;
	}

	template <typename T> void add_task_to_top(T && task) {
		auto lock = std::unique_lock(mutex);
		queue.emplace_back(std::forward<T>(task));
		cv.notify_one();
	}

	void add_threads(size_t count) {
		for (size_t i = 0; i != count; ++i) {
			add_thread();
		}
	}

	void stop_thread() {
		add_task_to_top([] { throw stop_exception{}; });
	}

	void stop_all_threads() {
		for (auto & th: threads) {
			stop_thread();
		}
	}

	void add_thread() {
		threads.emplace_back([this] {
			for (;;) {
				try {
					auto task = obtain_task_from_bottom();
					task();
				} catch (stop_exception) {
					break;
				}
			}
		});
	}
};

template <typename R> struct detached_task {
	struct promise_type {
		thread_pool & pool;
		std::coroutine_handle<> awaiter{};

		explicit promise_type(thread_pool & t) noexcept: pool{t} { }

		auto initial_suspend() {
			struct schedule_at_threadpool {
				bool await_ready() noexcept { return false; }
				void await_resume() noexcept { }
				void await_suspend(std::coroutine_handle<promise_type> h) {
					h.promise().pool.add_task_to_top([=] { h.resume(); });
				}
			};

			return schedule_at_threadpool{};
		}

		auto final_suspend() noexcept {
			struct suspend_or_jump_to_awaiter {
				bool await_ready() noexcept { return false; }
				void await_resume() noexcept { }
				auto await_suspend(std::coroutine_handle<promise_type> h) noexcept -> std::coroutine_handle<> {
					if (auto next = h.promise().awaiter) {
						std::cout << "final suspend -> awaiter\n";
						return next;
					} else {
						std::cout << "final suspend -> select()\n";
						return std::noop_coroutine(); // let thread_pool select next task
					}
				}
			};

			return suspend_or_jump_to_awaiter{};
		}

		void return_value(R) {
		}

		void unhandled_exception() {
			std::terminate();
		}

		auto get_return_object() {
			return std::coroutine_handle<promise_type>::from_promise(*this);
		}
	};

	using handle_type = std::coroutine_handle<promise_type>;

	handle_type handle{};

	detached_task(handle_type h) noexcept: handle{h} {
	}

	bool await_ready() const noexcept {
		assert(handle);
		return handle.done();
	}

	R await_resume() {
		return {};
	}

	void await_suspend(std::coroutine_handle<> awaiter) {
		std::cout << "awaiting...\n";
		// attach current task to follow after awaited ends
		handle.promise().awaiter = awaiter;
		// and let thread_pool obtain another task
	}
};

} // namespace co_curl

#endif
