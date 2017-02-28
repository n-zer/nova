#pragma once

#include <memory>
#include <vector>
#include "JobDataFD.h"
#include "JobFunction.h"
#include "JobCounterFD.h"

struct Job {
	JobFunction m_task;
	JobData* m_data;
	std::vector<std::shared_ptr<JobCounter>> m_counters;

	static void DeleteData(JobData* jd);
};
