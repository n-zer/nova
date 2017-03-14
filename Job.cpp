#include "Job.h"
#include "JobData.h"

void GenericJob::DeleteData(JobBase* jd) {
	delete jd;
}
