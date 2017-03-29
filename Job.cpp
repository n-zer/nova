#include "GenericJob.h"
#include "Job.h"

void GenericJob::DeleteData(JobBase* jd) {
	delete jd;
}
