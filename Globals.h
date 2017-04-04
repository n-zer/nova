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

template <typename Runnable>
void RunRunnable(void * runnable) {
	(static_cast<Runnable*>(runnable))->operator()();
}

template <typename Runnable>
void RunBatchRunnable(void* runnable) {
	Envelope* batchWrapper = static_cast<Envelope*>(runnable);
	unsigned int start = batchWrapper->Start();
	unsigned int end = batchWrapper->End();
	unsigned int count = end - start;
	unsigned int section = (WorkerThread::GetThreadId() + WorkerThread::GetThreadCount() - batchWrapper->startThread) % WorkerThread::GetThreadCount() + 1;
	unsigned int newStart = static_cast<unsigned int>(start + floorf(count*(section - 1) / batchWrapper->sections));
	end = static_cast<unsigned int>(start + floorf(count*section / batchWrapper->sections));

	std::tuple<Ts...> splitTuple = std::get<Ts...>(batchWrapper->job.m_tuple);
	std::tuple<unsigned int, unsigned int, Ts...> callParams = std::tuple_cat(std::make_tuple(newStart, end), splitTuple);
	apply(batchWrapper->job, callParams);
}

template <typename Runnable>
void DeleteRunnable(void * runnable) {
	delete static_cast<Runnable*>(runnable);
}