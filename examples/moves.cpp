#include <co_curl/co_curl.hpp>
#include <exception>

struct move_only_type {
	move_only_type() noexcept { }
	move_only_type(const move_only_type &) = delete;
	move_only_type(move_only_type && other) noexcept {};

	move_only_type & operator=(move_only_type &&) noexcept {
		return *this;
	}

	move_only_type & operator=(const move_only_type & other) noexcept = delete;
};

auto test1() -> co_curl::task<move_only_type> {
	move_only_type out{};
	co_return out;
}

auto test2() -> co_curl::task<move_only_type> {
	move_only_type out{};
	co_return std::move(out);
}

// auto test3() -> co_curl::task<move_only_type> {
//	const move_only_type out{};
//	co_return out;
// }
//
// auto test4() -> co_curl::task<move_only_type> {
//	const move_only_type out{};
//	co_return std::move(out);
// }

auto test5() -> co_curl::task<std::optional<move_only_type>> {
	co_return move_only_type{};
}

auto example() -> co_curl::task<void> {
	co_await test1();
	co_await test2();
	// co_await test3();
	// co_await test4();

	auto ta = test1();
	auto & a = ta;
	const auto tb = test2();
	auto & b = tb;
	auto c = co_await test5();
}

int main() {
	const auto ct = test1();
	auto t = test1();
	const move_only_type & a = ct;
	// move_only_type & b = ct;
	// move_only_type & c = test1();
	const move_only_type & c = test1(); // dangerous?
	move_only_type d = test1();
	// move_only_type e = t;
}