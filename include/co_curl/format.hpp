#ifndef CO_CURL_FORMAT_HPP
#define CO_CURL_FORMAT_HPP

#include <array>
#include <ostream>
#include <span>
#include <string_view>
#include <charconv>
#include <chrono>

namespace co_curl {

struct to_chars_result {
	std::string_view out;
	std::errc ec;

	constexpr operator bool() const noexcept {
		return ec == std::errc{};
	}
};

template <typename T, typename... Args> constexpr auto to_chars(std::span<char> output, T val, Args... args) {
	const auto r = std::to_chars(output.data(), output.data() + output.size(), val, args...);
	return to_chars_result{std::string_view(output.data(), r.ptr), r.ec};
}

struct data_amount {
	size_t amount{0};

	explicit(false) constexpr data_amount(std::same_as<size_t> auto n) noexcept: amount{n} { }

	static constexpr auto add_to_buffer(std::span<char> buffer, std::span<const char> used, std::string_view text) noexcept -> std::span<const char> {
		assert(used.data() == buffer.data());
		assert(used.size() <= buffer.size());

		const auto remaining = buffer.subspan(used.size());

		if (remaining.size() < text.size()) {
			return {};
		}

		std::copy(text.begin(), text.end(), remaining.begin());

		return std::span<const char>(buffer).first(used.size() + text.size());
	}

	constexpr auto write_into(std::span<char> buffer) const noexcept -> std::span<const char> {
		if (amount < 1024u) {
			const auto r = to_chars(buffer, amount);
			return add_to_buffer(buffer, r.out, " B");
		} else if (amount < (1024u * 1024u)) {
			const auto r = to_chars(buffer, static_cast<double>(amount) / 1024u, std::chars_format::fixed, 2);
			return add_to_buffer(buffer, r.out, " kiB");
		} else if (amount < (1024u * 1024u * 1024u)) {
			const auto r = to_chars(buffer, static_cast<double>(amount) / (1024u * 1024u), std::chars_format::fixed, 2);
			return add_to_buffer(buffer, r.out, " MiB");
		} else {
			const auto r = to_chars(buffer, static_cast<double>(amount) / (1024u * 1024u * 1024u), std::chars_format::fixed, 2);
			return add_to_buffer(buffer, r.out, " GiB");
		}
	}

	friend auto operator<<(std::ostream & os, data_amount in) -> std::ostream & {
		std::array<char, 32> buffer{};
		const auto r = in.write_into(buffer);
		os.write(buffer.data(), r.size());
		return os;
	}
};

struct duration {
	std::chrono::nanoseconds value;

	friend auto operator<<(std::ostream & os, duration in) -> std::ostream & {
		if (in.value < std::chrono::microseconds{2}) {
			return os << std::chrono::duration_cast<std::chrono::nanoseconds>(in.value).count() << " ns";
		} else if (in.value < std::chrono::milliseconds{2}) {
			return os << std::chrono::duration_cast<std::chrono::microseconds>(in.value).count() << " us";
		} else if (in.value < std::chrono::seconds{2}) {
			return os << std::chrono::duration_cast<std::chrono::milliseconds>(in.value).count() << " ms";
		} else if (in.value < std::chrono::minutes{2}) {
			return os << std::chrono::duration_cast<std::chrono::seconds>(in.value).count() << " s";
		} else if (in.value < std::chrono::hours{2}) {
			const auto minutes = std::chrono::floor<std::chrono::minutes>(in.value);
			const auto seconds = std::chrono::ceil<std::chrono::seconds>(in.value - minutes);
			return os << minutes.count() << " min " << seconds.count() << " s";
		} else {
			const auto hours = std::chrono::floor<std::chrono::hours>(in.value);
			const auto minutes = std::chrono::ceil<std::chrono::minutes>(in.value - hours);
			return os << hours.count() << " h " << minutes.count() << " min";
		}
	}
};

} // namespace co_curl

#endif
