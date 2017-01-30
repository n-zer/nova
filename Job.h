#pragma once

#include "JobCounter.h"

struct Job {
	void (*m_task)(void*);
	void* m_data;
	JobCounter* m_counter;
};
