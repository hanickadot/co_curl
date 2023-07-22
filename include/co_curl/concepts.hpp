#ifndef CO_CURL_CONCEPTS_HPP
#define CO_CURL_CONCEPTS_HPP

#include <ostream>
#include <concepts>

namespace co_curl {

template <typename T> concept ostreamable = requires(std::ostream & os, const T & val) {
	{ os << val } -> std::same_as<std::ostream &>;
};

}

#endif
