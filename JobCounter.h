#pragma once

#include <atomic>
#include "PushFunction.h"
#include "JobFD.h"

class JobCounter {
public:
	JobCounter();
	JobCounter(PushFunction, Job);
	~JobCounter();
private:
	PushFunction m_function;
	Job m_job;
};
