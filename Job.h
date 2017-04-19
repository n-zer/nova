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

	template <typename Callable, typename ... Ts>
	class BatchJob {
	public:
		BatchJob(Callable callable, unsigned start, unsigned end, Ts... args)
			: m_callable(callable), m_tuple(start, end, args...) {
			unsigned int count = end - start;

			m_sections = internal::WorkerThread::GetThreadCount();
			if (count < m_sections)
				m_sections = count;
		}

		void operator () () {
			std::tuple<unsigned, unsigned, Ts...> params = m_tuple;
			unsigned int start = std::get<0>(params);
			unsigned int end = std::get<1>(params);
			float count = static_cast<float>(end - start);
			unsigned int section = InterlockedIncrement(&m_currentSection);
			unsigned int newStart = static_cast<unsigned>(start + floorf(static_cast<float>(count*(section - 1) / m_sections)));
			end = static_cast<unsigned int>(start + floorf(count*section / m_sections));

			std::get<0>(params) = newStart;
			std::get<1>(params) = end;
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
		std::tuple<unsigned, unsigned, Ts...> m_tuple;
	};

	template <typename Obj, typename ... Ts>
	class MemberFunctionWrapper {
	public:
		MemberFunctionWrapper(Obj obj, void(Obj::*func)(Ts...))
			: m_func(func), m_obj(obj) {

		}

		void operator() (Ts... args) {
			(m_obj.*m_func)(args...);
		}
	private:
		void (Obj::*m_func)(Ts... args);
		Obj m_obj;
	};


	template <typename Callable, typename ... Ts>
	SimpleJob<Callable, Ts...> MakeJob(Callable callable, Ts... args) {
		//To-do, insert SFINAE check for () operator on callable
		return SimpleJob<Callable, Ts...>(callable, args...);
	}

	template <typename Callable, typename ... Ts>
	BatchJob<Callable, Ts...> MakeBatchJob(Callable callable, unsigned start, unsigned end, Ts... args) {
		//To-do, insert SFINAE check for () operator on callable
		return BatchJob<Callable, Ts...>(callable, start, end, args...);
	}

	template <typename Obj, typename ... Ts>
	SimpleJob<MemberFunctionWrapper<Obj, Ts...>, Ts...> MakeJob(void(Obj::*func)(Ts...), Obj obj, Ts... args) {
		return SimpleJob<MemberFunctionWrapper<Obj, Ts...>, Ts...>({ obj,func }, args...);
	}

	template <typename Obj, typename ... Ts>
	BatchJob<MemberFunctionWrapper<Obj, Ts...>, Ts...> MakeBatchJob(void(Obj::*func)(Ts...), Obj obj, unsigned start, unsigned end, Ts... args) {
		return BatchJob<MemberFunctionWrapper<Obj, unsigned, unsigned, Ts...>, Ts...>({ obj,func }, start, end, args...);
	}
}
