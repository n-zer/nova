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

template <typename T>
struct BatchWrapper : JobBase {
	T job;
	unsigned int startThread;
	unsigned int sections;
	BatchWrapper(T obj, unsigned int start, unsigned int sections) : job(obj), startThread(start), sections(sections) {}
};

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
class SimpleJob<Callable, unsigned, unsigned, Ts...> {
public:
	SimpleJob(Callable callable, unsigned start, unsigned end, Ts... args)
		: m_callable(callable), m_start(start), m_end(end), m_tuple(args...) {

	}

	unsigned Start() { return m_start; }
	unsigned End() { return m_end; }

	void operator () () {
		apply(*this, m_tuple);
	}

	void operator() (Ts... args) {
		this->operator()(m_start, m_end, args...);
	}

	void operator() (unsigned start, unsigned end, Ts... args) {
		m_callable(start, end, args...);
	}

private:
	unsigned m_start;
	unsigned m_end;
	Callable m_callable;
	std::tuple<Ts...> m_tuple;
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

template <typename Obj, typename ... Ts>
SimpleJob<MemberFunctionWrapper<Obj, Ts...>, Ts...> MakeJob(void(Obj::*func)(Ts...), Obj obj, Ts... args) {
	return SimpleJob<MemberFunctionWrapper<Obj, Ts...>, Ts...>({ obj,func }, args...);
}
