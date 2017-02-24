#pragma once

#include <memory>
#include <vector>

#include "JobDataFD.h"
#include "JobCounter.h"

struct JobData {
	virtual ~JobData() {};
};

struct CountableJobData : JobData {
	 vector<shared_ptr<JobCounter>> m_counters;
};

struct BatchJobData : JobData {
	unsigned int start;
	unsigned int count;
	CountableJobData* jobData;
};
