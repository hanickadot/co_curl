#ifndef CO_CURL_ALL_HPP
#define CO_CURL_ALL_HPP

#include "promise.hpp"
#include <ranges>
#include <tuple>
#include <vector>

namespace co_curl {

// TODO for now I react only on tasks
struct match_task {
	template <typename... Ts> static auto test(co_curl::promise<Ts...> &) -> std::true_type;
	template <typename Y> static auto test(Y &) -> std::false_type;
};

template <typename T> concept type_is_task = bool(decltype(match_task::test(std::declval<T &>()))());

struct match_optional {
	template <typename... Ts> static auto test(std::optional<Ts...> &) -> std::true_type;
	template <typename Y> static auto test(Y &) -> std::false_type;
};

template <typename T> concept type_is_optional = bool(decltype(match_optional::test(std::declval<T &>()))());

template <typename R> concept range_of_tasks = std::ranges::range<R> && type_is_task<std::ranges::range_value_t<R>>;

template <range_of_tasks R> using range_of_tasks_result = typename std::ranges::range_value_t<R>::return_type;
template <range_of_tasks R> using range_of_tasks_optional_result = typename std::ranges::range_value_t<R>::return_type::value_type;

struct all_helper {
	template <range_of_tasks R> auto operator()(R && tasks) const -> co_curl::promise<std::vector<range_of_tasks_result<R>>> {
		static_assert(!type_is_optional<range_of_tasks_result<R>>);

		using value_type = std::ranges::range_value_t<R>;
		using task_result_type = range_of_tasks_result<R>;
		// identify<task_result_type> i;

		// we need to materialize to start tasks...
		// TODO replace with ranges::to
		std::vector<value_type> materialized{};
		for (auto && task: tasks) {
			materialized.emplace_back(std::move(task));
		}

		std::vector<task_result_type> output{};
		output.reserve(materialized.size());

		for (auto && task: materialized) {
			output.emplace_back(co_await std::move(task));
		}

		co_return output;
	}

	template <range_of_tasks R> friend auto operator|(R && tasks, all_helper self) {
		return self(std::forward<R>(tasks));
	}

	template <range_of_tasks R> requires(type_is_optional<range_of_tasks_result<R>>) auto operator()(R && tasks) const -> co_curl::promise<std::optional<std::vector<range_of_tasks_optional_result<R>>>> {
		static_assert(type_is_optional<range_of_tasks_result<R>>);
		using value_type = std::ranges::range_value_t<R>;
		using task_result_type = range_of_tasks_optional_result<R>;
		// identify<task_result_type> i;

		// we need to materialize to start tasks...
		// TODO replace with ranges::to
		std::vector<value_type> materialized{};
		for (auto && task: tasks) {
			materialized.emplace_back(std::move(task));
		}

		auto output = std::optional<std::vector<task_result_type>>(std::in_place);
		output->reserve(materialized.size());

		for (auto && task: materialized) {
			auto r = co_await std::move(task);

			if (!output) {
				continue;
			}

			if (!r.has_value()) {
				output = std::nullopt;
				continue;
			}

			output->emplace_back(std::move(*r));
		}

		co_return output;
	}

	template <range_of_tasks R> requires(type_is_optional<range_of_tasks_result<R>>) friend auto operator|(R && tasks, all_helper self) {
		return self(std::forward<R>(tasks));
	}

	template <typename... Ts, typename... Scheduler> auto operator()(co_curl::promise<Ts, Scheduler> &&... promises) const -> co_curl::promise<std::tuple<Ts...>> {
		co_return std::tuple<Ts...>{(co_await std::move(promises))...};
	}

	template <typename... Ts, typename... Scheduler> auto operator()(co_curl::promise<std::optional<Ts>, Scheduler> &&... promises) const -> co_curl::promise<std::optional<std::tuple<Ts...>>> {
		auto tmp = std::tuple<std::optional<Ts>...>{(co_await std::move(promises))...};

		co_return [&]<size_t... Idx>(std::index_sequence<Idx...>) -> std::optional<std::tuple<Ts...>> {
			if (((!std::get<Idx>(tmp).has_value()) || ... || false)) {
				return std::nullopt;
			}

			return std::tuple<Ts...>{std::move(*std::get<Idx>(tmp))...};
		}(std::make_index_sequence<sizeof...(Ts)>());
	}
};

static constexpr auto all = all_helper{};

} // namespace co_curl

#endif
