#ifndef CO_CURL_URL_HPP
#define CO_CURL_URL_HPP

#include <optional>
#include <string>

// same as in curl library
typedef struct Curl_URL CURLU;

namespace co_curl {

struct url {
	CURLU * handle;

	url();
	explicit url(const char * cstr);
	url(const char * a, const char * b);
	url(const url &);
	~url() noexcept;

	url & remove_fragment();

	url & operator=(const char * cstr);

	std::optional<std::string> get() const;
	std::optional<std::string> path() const;
};

} // namespace co_curl

#endif
