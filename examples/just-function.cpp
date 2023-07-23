#include <co_curl/function.hpp>
#include <iostream>

// this looks like a coroutine, but it's just a function modeled with courutines
auto just_function() -> co_curl::function<int> {
	co_return 42;
}

int main() {
	std::cout << just_function() << "\n";
}