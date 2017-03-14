#pragma once
#include <algorithm>
#include <functional>
#include <utility>


template <typename T>
T clip(const T& n, const T& lower, const T& upper) {
	return max(lower, min(n, upper));
}

namespace detail {
	template <class F, class Tuple, std::size_t... I>
	constexpr decltype(auto) apply_impl(F &&f, Tuple &&t, std::index_sequence<I...>)
	{
		return std::invoke(std::forward<F>(f), std::get<I>(std::forward<Tuple>(t))...);
	}
}  // namespace detail

template <class F, class Tuple>
constexpr decltype(auto) apply(F &&f, Tuple &&t)
{
	return detail::apply_impl(
		std::forward<F>(f), std::forward<Tuple>(t),
		std::make_index_sequence<std::tuple_size_v<std::decay_t<Tuple>>>{});
}

//template <typename F, typename Tuple, size_t... I>
//decltype(auto) apply_impl(F&& f, Tuple&& t, std::index_sequence<I...>) {
//	return std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...);
//}
//
//template <typename F, typename Tuple>
//decltype(auto) apply(F&& f, Tuple&& t) {
//	using Indices = std::make_index_sequence<tuple_size<decay_t<Tuple>>::value>;
//	return apply_impl(std::forward<F>(f), std::forward<Tuple>(t), Indices{});
//}
