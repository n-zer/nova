#pragma once
#include <Windows.h>

namespace Nova {
	namespace internal {
		template <typename T>
		T clip(const T& n, const T& lower, const T& upper) {
			return (std::max)(lower, (std::min)(n, upper));
		}

		namespace detail {
			template <class F, class Tuple, std::size_t... I>
			constexpr decltype(auto) apply_impl(F &&f, Tuple &&t, std::index_sequence<I...>) {
				return std::invoke(std::forward<F>(f), std::get<I>(std::forward<Tuple>(t))...);
			}
		}  // namespace detail

		template <class F, class Tuple>
		constexpr decltype(auto) apply(F &&f, Tuple &&t) {
			return detail::apply_impl(
				std::forward<F>(f), std::forward<Tuple>(t),
				std::make_index_sequence<std::tuple_size_v<std::decay_t<Tuple>>>{});
		}

		template <class T, class Tuple>
		struct Index;

		template <class T, class... Types>
		struct Index<T, std::tuple<T, Types...>> {
			static const std::size_t value = 0;
		};

		template <class T, class U, class... Types>
		struct Index<T, std::tuple<U, Types...>> {
			static const std::size_t value = 1 + Index<T, std::tuple<Types...>&>::value;
		};

		template <class T, class... Types>
		struct Index<T, std::tuple<T, Types...>&> {
			static const std::size_t value = 0;
		};

		template <class T, class U, class... Types>
		struct Index<T, std::tuple<U, Types...>&> {
			static const std::size_t value = 1 + Index<T, std::tuple<Types...>&>::value;
		};
	}

	//Starts the job system. Pass in a callable object and some parameters.
	template <typename Callable, typename ... Params>
	static void Start(Callable callable, Params ... args) {
		Start(std::thread::hardware_concurrency(), callable, args...);
	}

	//Starts the job system. Pass in a callable object and some parameters.
	template <typename Callable, typename ... Params>
	static void Start(unsigned threadCount, Callable callable, Params ... args) {
		using namespace internal;

		//create threads
		std::vector<WorkerThread> threads;

		threads.resize(threadCount - 1);

		ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);

		Push<true>(MakeJob(callable, args...));

		WorkerThread::JobLoop();

		/*for (unsigned c = 0; c < WorkerThread::GetThreadCount() - 1; c++)
			Push(MakeJob(&WorkerThread::KillWorker));

		for (WorkerThread& w : threads)
			w.Join();*/
	}
}