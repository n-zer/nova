#pragma once

#include <memory>
#include <vector>
#include <tuple>

#include "JobFD.h"

struct JobBase {
	JobBase() {};
	virtual ~JobBase() {};
};

template <typename ... Ts>
struct Job : JobBase {
	void(*m_func)(Ts...);
	std::tuple<Ts...> m_tuple;
	Job(void(*func)(Ts...), Ts... args) {
		m_func = func;
		m_tuple = std::make_tuple(args...);
	}
};

template <typename ... Ts>
struct BatchJob : Job<Ts...> {
	unsigned int startThread;
	unsigned int sections;
	BatchJob(Job<Ts...> base, unsigned int start, unsigned int sections) : Job<Ts...>(base), startThread(start), sections(sections) {}
};
