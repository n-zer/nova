#pragma once

#include <memory>
#include <vector>

#include "JobDataFD.h"
#include "JobCounter.h"

struct JobData {
	virtual ~JobData() {};
};

struct PrintData : JobData {
	long long data;
	PrintData(long long data) : data(data) {};
};

struct BatchJobData : JobData {
	unsigned int start;
	unsigned int count;
	JobData* data;
	BatchJobData(unsigned int start, unsigned int count, JobData* jd) : start(start), count(count), data(jd) {};
	~BatchJobData() { delete data; };
};
