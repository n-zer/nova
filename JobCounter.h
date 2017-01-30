#pragma once

#include <atomic>
#include "JobQueue.h"

class JobCounter {
public:
	JobCounter();
	JobCounter(PushFunction, Job, unsigned int);
	void Init(PushFunction, Job, unsigned int);
	void Increment(unsigned int);
private:
	std::atomic<unsigned int> m_counter;
	unsigned int m_limit;
	PushFunction m_function;
	Job m_job;
};
