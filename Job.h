#pragma once

#include "JobCounter.h"
#include "JobData.h"

typedef void(*JobFunction)(JobData*);

struct Job {
	JobFunction m_task;
	JobData* m_data;
};
