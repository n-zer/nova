#pragma once

#include <vector>
#include <tuple>
#include <stdint.h>
#include <Windows.h>
#include <type_traits>
#include <algorithm>

namespace Nova {
	template<typename Callable, typename ... Params>
	class BatchJob;

	template <typename Callable, typename ... Params>
	class SimpleJob {
	public:
		SimpleJob(Callable callable, Params... args)
			: m_callable(callable), m_tuple(args...) {
		}

		void operator () () const {
			internal::apply(m_callable, m_tuple);
		}

		//Ignore the squiggly, this is defined further down
		operator BatchJob<Callable, Params...>() const;

	protected:
		Callable m_callable;
		std::tuple<Params...> m_tuple;
	};

	typedef unsigned BatchIndex;

	template <typename Callable, typename ... Params>
	class BatchJob : public SimpleJob<Callable, Params...> {
	public:
		BatchJob(Callable callable, Params... args)
			: SimpleJob<Callable, Params...>(callable, args...), m_sections((std::min)(End() - Start(), internal::WorkerThread::GetThreadCount())) {
		}

		BatchJob(SimpleJob<Callable, Params...> & sj)
			: SimpleJob<Callable, Params...>(sj), m_sections((std::min)(End() - Start(), internal::WorkerThread::GetThreadCount())) {
		}

		void operator () () {
			std::tuple<Params...> params = m_tuple;
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

		unsigned GetSections() const {
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

	template<typename Callable, typename ...Params>
	inline SimpleJob<Callable, Params...>::operator BatchJob<Callable, Params...>() const {
		return BatchJob<Callable, Params...>(*this);
	}


	template <typename Callable, typename ... Params>
	SimpleJob<Callable, Params...> MakeJob(Callable callable, Params... args) {
		return SimpleJob<Callable, Params...>(callable, args...);
	}

	template <typename Callable, typename ... Params>
	BatchJob<Callable, Params...> MakeBatchJob(Callable callable, Params... args) {
		return BatchJob<Callable, Params...>(callable, args...);
	}
}
