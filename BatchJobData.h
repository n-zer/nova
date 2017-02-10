#pragma once

#include "CountableJobData.h"

struct BatchJobData : CountableJobData {
	unsigned int start;
	unsigned int count;
};
