#pragma once

#include "Job.h"
#include "JobFunction.h"
#include "JobDataFD.h"

class JobCounter {
public:
	JobCounter(GenericJob);
	JobCounter(JobFunction, JobBase*);
	~JobCounter();
private:
	GenericJob m_job;
};
