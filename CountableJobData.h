#pragma once

#include "JobData.h"
#include "JobCounter.h"

struct CountableJobData : JobData {
	JobCounter m_counter;
};
