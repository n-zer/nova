#pragma once

#include <memory>
#include <vector>
#include <tuple>

#include "JobFD.h"

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
