#pragma once

#include <atomic>
#include "Job.h"
#include "JobFunction.h"
#include "JobDataFD.h"

class JobCounter {
public:
	JobCounter(Job);
	JobCounter(JobFunction, JobData*);
	~JobCounter();
private:
	Job m_job;
};
