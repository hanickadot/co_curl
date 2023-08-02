#ifndef CO_CURL_CONCEPTS_HPP
#define CO_CURL_CONCEPTS_HPP

#include <ostream>
#include <concepts>

namespace co_curl {

template <typename T, typename Stream> concept ostreamable = requires(std::basic_ostream<Stream> & os, const T & val) {
	{ os << val } -> std::same_as<std::basic_ostream<Stream> &>;
};

}

#endif
