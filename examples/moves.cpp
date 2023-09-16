#include <co_curl/co_curl.hpp>
#include <exception>

struct move_only_type {
	move_only_type() noexcept { }
	move_only_type(const move_only_type &) = delete;
	move_only_type(move_only_type &&) noexcept {};

	move_only_type & operator=(move_only_type &&) noexcept {
		return *this;
	}

	move_only_type & operator=(const move_only_type &) noexcept = delete;
};

auto test1() -> co_curl::promise<move_only_type> {
	move_only_type out{};
	co_return out;
}

auto test2() -> co_curl::promise<move_only_type> {
	move_only_type out{};
	co_return std::move(out);
}

// auto test3() -> co_curl::promise<move_only_type> {
//	const move_only_type out{};
//	co_return out;
// }
//
// auto test4() -> co_curl::promise<move_only_type> {
//	const move_only_type out{};
//	co_return std::move(out);
// }

auto test5() -> co_curl::promise<std::optional<move_only_type>> {
	co_return move_only_type{};
}

auto example() -> co_curl::promise<void> {
	co_await test1();
	co_await test2();
	// co_await test3();
	// co_await test4();

	[[maybe_unused]] auto ta = test1();
	[[maybe_unused]] auto & a = ta;
	[[maybe_unused]] const auto tb = test2();
	[[maybe_unused]] auto & b = tb;
	[[maybe_unused]] auto c = co_await test5();
}

int main() {
	[[maybe_unused]] const auto ct = test1();
	[[maybe_unused]] auto t = test1();
	[[maybe_unused]] const move_only_type & a = ct;
	// move_only_type & b = ct;
	// move_only_type & c = test1();
	[[maybe_unused]] const move_only_type & c = test1(); // dangerous?
	[[maybe_unused]] move_only_type d = test1();
	// move_only_type e = t;
}