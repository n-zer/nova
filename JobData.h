#pragma once

#include <memory>
#include "JobDataFD.h"
#include "JobCounter.h"

struct JobData {
	virtual ~JobData() {};
};

struct CountableJobData : JobData {
	 shared_ptr<JobCounter> m_counter;
};

struct BatchJobData : CountableJobData {
	unsigned int start;
	unsigned int count;
};
