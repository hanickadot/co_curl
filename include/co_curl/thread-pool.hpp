#ifndef CO_CURL_THREAD_POOL_HPP
#define CO_CURL_THREAD_POOL_HPP

#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <queue>
#include <thread>
#include <utility>
#include <variant>
#include <vector>
#include <cassert>
#include <coroutine>

namespace co_curl {

struct thread_pool {
	std::queue<std::coroutine_handle<>> queue{};
	std::vector<std::thread> threads{};
	std::mutex mutex{};
	std::condition_variable cv{};

	explicit thread_pool(size_t count) {
		add_threads(count);
	}

	thread_pool(const thread_pool &) = delete;
	thread_pool(thread_pool &&) = delete;

	~thread_pool() noexcept {
		stop_and_join();
	}

	void stop_and_join() {
		for (std::thread & th: threads) {
			add({});
		}

		for (std::thread & th: threads) {
			th.join();
		}
		threads.resize(0);
	}

	auto get_next() -> std::coroutine_handle<> {
		auto lock = std::unique_lock(mutex);
		cv.wait(lock, [this] { return !queue.empty(); });

		auto res = std::move(queue.front());
		queue.pop();
		return res;
	}

	void add(std::coroutine_handle<> handle) {
		auto lock = std::unique_lock(mutex);
		queue.emplace(handle);
		cv.notify_one();
	}

	void add_threads(size_t count) {
		for (size_t i = 0; i != count; ++i) {
			add_thread();
		}
	}

	void add_thread() {
		threads.emplace_back([this] {
			while (auto handle = get_next()) {
				handle.resume();
			}
		});
	}
};

template <typename CoroType = void, typename... Args> struct give_me_coroutine_handle {
	using promise_type = typename std::coroutine_traits<CoroType, Args...>::promise_type;
	std::coroutine_handle<promise_type> handle{};

	bool await_ready() const noexcept { return false; }
	bool await_suspend(std::coroutine_handle<promise_type> h) noexcept {
		handle = h;
		return false;
	}
	auto await_resume() const noexcept {
		return handle;
	}
};

struct move_to_another_thread {
	std::thread & th;
	bool await_ready() const noexcept { return false; }
	void await_suspend(std::coroutine_handle<> h) noexcept {
		th = std::thread([h] { h.resume(); });
	}
	void await_resume() const noexcept { }
};

template <typename R> struct result_or_exception: std::variant<std::monostate, R, std::exception_ptr> {
	using super = std::variant<std::monostate, R, std::exception_ptr>;
	using super::super;

	R result() && {
		if (super::index() == 1) {
			return std::move(std::get<1>(*this));
		} else if (super::index() == 2) {
			std::rethrow_exception(std::get<2>(*this));
		} else {
			assert(false);
			std::terminate();
		}
	}
	R & result() & {
		if (super::index() == 1) {
			return std::get<1>(*this);
		} else if (super::index() == 2) {
			std::rethrow_exception(std::get<2>(*this));
		} else {
			assert(false);
			std::terminate();
		}
	}
};

template <> struct result_or_exception<void>: std::variant<std::monostate, std::exception_ptr> {
	using super = std::variant<std::monostate, std::exception_ptr>;
	using super::super;

	void result() {
		if (super::index() == 0) {
			return;
		} else {
			std::rethrow_exception(std::get<1>(*this));
		}
	}
};

template <typename Promise> concept with_thread_pool = requires(Promise & promise) {
	{ promise.pool } -> std::same_as<thread_pool &>;
};

struct schedule_at_threadpool {
	bool await_ready() noexcept { return false; }
	void await_resume() noexcept { }
	template <with_thread_pool Promise> void await_suspend(std::coroutine_handle<Promise> h) {
		h.promise().pool.add(h);
	}
};

template <typename Promise> concept with_single_awaiter = requires(Promise & promise) {
	{ promise.awaiter } -> std::convertible_to<std::coroutine_handle<>>;
};

struct suspend_or_jump_to_awaiter {
	bool await_ready() noexcept { return false; }

	void await_resume() noexcept { }

	template <with_single_awaiter Promise> auto await_suspend(std::coroutine_handle<Promise> h) noexcept -> std::coroutine_handle<> {
		if (auto next = h.promise().awaiter) {
			return next;
		} else {
			return std::noop_coroutine(); // let thread_pool select next task
		}
	}

	// sink for other promise types
	void await_suspend(std::coroutine_handle<>) noexcept { }
};

template <typename R> struct detached_promise_type {
	thread_pool & pool;
	std::coroutine_handle<> awaiter{};
	result_or_exception<R> result{};

	explicit detached_promise_type(thread_pool & t) noexcept: pool{t} { }

	auto initial_suspend() {
		return schedule_at_threadpool{};
	}

	auto final_suspend() noexcept {
		return suspend_or_jump_to_awaiter{};
	}

	void hello() {
		std::cout << "hello\n";
	}

	template <std::convertible_to<R> T> void return_value(T && value) {
		result = std::forward<T>(value);
	}

	void unhandled_exception() noexcept {
		result = std::current_exception();
	}

	auto get_return_object() {
		return std::coroutine_handle<detached_promise_type>::from_promise(*this);
	}
};

template <> struct detached_promise_type<void> {
	thread_pool & pool;
	std::coroutine_handle<> awaiter{};
	result_or_exception<void> result{};

	explicit detached_promise_type(thread_pool & t) noexcept: pool{t} { }

	auto initial_suspend() {
		return schedule_at_threadpool{};
	}

	auto final_suspend() noexcept {
		return suspend_or_jump_to_awaiter{};
	}

	void return_void() {
		result = std::monostate{};
	}

	void unhandled_exception() noexcept {
		result = std::current_exception();
	}

	auto get_return_object() {
		return std::coroutine_handle<detached_promise_type>::from_promise(*this);
	}
};

template <typename R> struct detached_task {
	using promise_type = detached_promise_type<R>;
	using handle_type = std::coroutine_handle<promise_type>;

	handle_type handle{};

	detached_task(handle_type h) noexcept: handle{h} { }
	detached_task(const detached_task &) = delete;
	detached_task(detached_task && other) noexcept: handle{std::exchange(other.handle, nullptr)} { }

	detached_task & operator=(const detached_task &) = delete;
	detached_task & operator=(detached_task && other) noexcept {
		std::swap(other.handle, handle);
		return *this;
	}

	~detached_task() noexcept {
		if (handle) {
			handle.destroy();
		}
	}

	bool await_ready() const noexcept {
		assert(handle);
		return handle.done();
	}

	R await_resume() {
		return handle.promise().result.result();
	}

	void await_suspend(std::coroutine_handle<> awaiter) {
		assert(handle.promise().awaiter == nullptr); // we support one awaiter for detached_tasks
		// attach current task to follow after awaited ends
		handle.promise().awaiter = awaiter;
		// and let thread_pool obtain another task
	}
};

template <typename T> struct sync_awaiter {
	struct promise_type {
		std::promise<T> result;

		auto initial_suspend() { return std::suspend_never{}; }
		auto final_suspend() noexcept { return std::suspend_always{}; }

		void return_value(T value) noexcept {
			result.set_value(value);
		}

		void unhandled_exception() noexcept {
			std::terminate();
		}

		auto get_return_object() {
			return std::coroutine_handle<promise_type>::from_promise(*this);
		}
	};

	using handle_type = std::coroutine_handle<promise_type>;
	handle_type handle{};

	sync_awaiter(std::coroutine_handle<promise_type> h) noexcept: handle{h} { }
	sync_awaiter(const sync_awaiter &) = delete;
	sync_awaiter(sync_awaiter &&) = delete;

	~sync_awaiter() noexcept {
		handle.destroy();
	}

	auto get_result() {
		return handle.promise().result.get_future().get();
	}
};

template <typename T> auto sync_await(detached_task<T> && task) {
	return [&]() -> sync_awaiter<T> { co_return co_await task; }().get_result();
}

template <typename T> auto sync_await(detached_task<T> & task) {
	return [&]() -> sync_awaiter<T> { co_return co_await task; }().get_result();
}

} // namespace co_curl

#endif
