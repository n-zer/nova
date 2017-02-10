#pragma once

#include "JobCounter.h"
#include "JobData.h"

struct Job {
	void (*m_task)(JobData*);
	JobData* m_data;
};
