#pragma once

#include <memory>
#include <vector>
#include <tuple>

#include "JobDataFD.h"

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
