#include "Job.h"
#include "JobData.h"

void Job::DeleteData(JobData* jd) {
	delete jd;
}

void Job::DeleteBatchData(JobData * jd)
{
	delete static_cast<BatchJobData*>(jd)->data;
	delete jd;
}
