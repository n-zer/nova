#pragma once

#include "GenericJob.h"
#include "JobFunction.h"
#include "JobFD.h"

class JobCounter {
public:
	JobCounter(GenericJob);
	JobCounter(JobFunction, JobBase*);
	~JobCounter();
private:
	GenericJob m_job;
};
