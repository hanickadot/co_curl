#ifndef CO_CURL_CO_CURL_HPP
#define CO_CURL_CO_CURL_HPP

#include "easy.hpp"
#include "multi.hpp"
#include "promise.hpp"
#include "scheduler.hpp"
#include <string_view>

namespace co_curl {

auto version() noexcept -> std::string_view;
auto curl_version() noexcept -> std::string_view;

void global_init() noexcept;

} // namespace co_curl

#endif
