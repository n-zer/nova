#pragma once

#include "JobDataFD.h"
#include "JobFunction.h"

struct Job {
	JobFunction m_task;
	JobData* m_data;
};
