#pragma once

#include <vector>
#include <tuple>
#include <stdint.h>
#include <Windows.h>
#include <type_traits>

namespace Nova {
	template <typename Callable, typename ... Ts>
	class SimpleJob {
	public:
		SimpleJob(Callable callable, Ts... args)
			: m_callable(callable), m_tuple(args...) {

		}

		void operator () () {
			internal::apply(m_callable, m_tuple);
		}

	private:
		Callable m_callable;
		std::tuple<Ts...> m_tuple;
	};

	typedef unsigned BatchIndex;

	template <typename Callable, typename ... Ts>
	class BatchJob {
	public:
		BatchJob(Callable callable, Ts... args)
			: m_callable(callable), m_tuple(args...) {
			unsigned int count = End() - Start();

			m_sections = internal::WorkerThread::GetThreadCount();
			if (count < m_sections)
				m_sections = count;
		}

		void operator () () {
			std::tuple<Ts...> params = m_tuple;
			BatchIndex start = Start();
			BatchIndex end = End();
			float count = static_cast<float>(end - start);
			unsigned int section = InterlockedIncrement(&m_currentSection);
			unsigned int newStart = static_cast<unsigned>(start + floorf(static_cast<float>(count*(section - 1) / m_sections)));
			end = static_cast<unsigned int>(start + floorf(count*section / m_sections));

			Start(params) = newStart;
			End(params) = end;
			internal::apply(m_callable, params);
		}

		unsigned GetSections() {
			return m_sections;
		}

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 32);
		}

		void operator delete(void* p)
		{
			_mm_free(p);
		}


	private:
		alignas(32) uint32_t m_currentSection = 0;
		unsigned m_sections;
		Callable m_callable;
		std::tuple<Ts...> m_tuple;

		unsigned& Start() {
			return Start(m_tuple);
		}
		static unsigned& Start(decltype(m_tuple) & tuple) {
			return std::get<internal::Index<BatchIndex, decltype(tuple)>::value>(tuple);
		}
		unsigned& End() {
			return End(m_tuple);
		}
		static unsigned& End(decltype(m_tuple) & tuple) {
			return std::get<internal::Index<BatchIndex, decltype(tuple)>::value + 1>(tuple);
		}
	};


	template <typename Callable, typename ... Ts>
	SimpleJob<Callable, Ts...> MakeJob(Callable callable, Ts... args) {
		return SimpleJob<Callable, Ts...>(callable, args...);
	}

	template <typename Callable, typename ... Ts>
	BatchJob<Callable, Ts...> MakeBatchJob(Callable callable, Ts... args) {
		return BatchJob<Callable, Ts...>(callable, args...);
	}
}
