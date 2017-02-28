#include "Job.h"
#include "JobData.h"

void Job::DeleteData(JobData* jd) {
	delete jd;
}