#pragma once

#include <memory>
#include <vector>
#include <tuple>

#include "JobFD.h"

#pragma region Old Job Classes

struct JobBase {
	JobBase() {};
	virtual ~JobBase() {};
};

template<typename ... Ts>
struct TemplatedJobBase : JobBase{
	std::tuple<Ts...> m_tuple;
	TemplatedJobBase(Ts... args) : m_tuple(args...) {}
	void virtual operator () (Ts... args) = 0;
};

template <typename ... Ts>
struct Job : TemplatedJobBase<Ts...> {
	void(*m_func)(Ts...);
	Job(void(*func)(Ts...), Ts... args) : TemplatedJobBase<Ts...>(args...), m_func(func) {}
	void operator () (Ts... args) {
		m_func(args...);
	}
};

template <typename T, typename ... Ts>
struct MemberJob : TemplatedJobBase<Ts...> {
	void(T::*m_func)(Ts...);
	T* m_obj;
	MemberJob(void(T::*func)(Ts...), T* obj, Ts... args) : TemplatedJobBase<Ts...>(args...), m_func(func), m_obj(obj) {}
	void operator () (Ts... args) {
		((*m_obj).*m_func)(args...);
	}
};

//template <typename T>
//struct BatchWrapper : JobBase {
//	T job;
//	unsigned int startThread;
//	unsigned int sections;
//	BatchWrapper(T obj, unsigned int start, unsigned int sections) : job(obj), startThread(start), sections(sections) {}
//};

#pragma endregion

template <typename Callable, typename ... Ts>
class SimpleJob {
public:
	SimpleJob(Callable callable, Ts... args)
		: m_callable(callable), m_tuple(args...) {

	}

	void operator () () {
		apply(m_callable, m_tuple);
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

		m_sections = WorkerThread::GetThreadCount();
		m_start = (WorkerThread::GetThreadId() + 1) % m_sections;
		if (count < m_sections)
			m_sections = count;
	}

	void operator () () {
		std::tuple<unsigned, unsigned, Ts...> params = m_tuple;
		unsigned int start = std::get<0>(params);
		unsigned int end = std::get<1>(params);
		unsigned int count = end - start;
		unsigned int section = (WorkerThread::GetThreadId() + WorkerThread::GetThreadCount() - m_start) % WorkerThread::GetThreadCount() + 1;
		unsigned int newStart = static_cast<unsigned int>(start + floorf(count*(section - 1) / m_sections));
		end = static_cast<unsigned int>(start + floorf(count*section / m_sections));

		std::get<0>(params) = newStart;
		std::get<1>(params) = end;
		apply(m_callable, params);
	}

	unsigned GetSections() {
		return m_sections;
	}


private:
	unsigned m_start;
	unsigned m_sections;
	Callable m_callable;
	std::tuple<unsigned, unsigned, Ts...> m_tuple;
};


//template <typename Batchable>
//struct BatchWrapper {
//	Batchable m_callable;
//	unsigned int startThread;
//	unsigned int sections;
//	BatchWrapper(Batchable callable, unsigned int start, unsigned int sections) : m_callable(callable), startThread(start), sections(sections) {}
//	void operator () () {
//		decltype(m_callable.Params()) params = m_callable.Params();
//		unsigned int start = std::get<0>(params);
//		unsigned int end = std::get<1>(params);
//		unsigned int count = end - start;
//		unsigned int section = (WorkerThread::GetThreadId() + WorkerThread::GetThreadCount() - startThread) % WorkerThread::GetThreadCount() + 1;
//		unsigned int newStart = static_cast<unsigned int>(start + floorf(count*(section - 1) / sections));
//		end = static_cast<unsigned int>(start + floorf(count*section / sections));
//
//		std::get<0>(params) = newStart;
//		std::get<1>(params) = end;
//		apply(m_callable, params);
//	}
//};


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
