#pragma once

#include <atomic>
#include "JobQueue.h"

class JobCounter {
public:
	JobCounter();
	JobCounter(PushFunction, Job);
	JobCounter(const JobCounter & other);
	JobCounter& operator=(const JobCounter & other);
	JobCounter(const JobCounter && other);
	JobCounter& operator=(const JobCounter && other);
	~JobCounter();
private:
	std::atomic<unsigned int> * m_counter;
	PushFunction m_function;
	Job m_job;
};
