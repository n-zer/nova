#include "JobCounter.h"
#include "GenericJob.h"
#include "Job.h"

JobCounter::JobCounter(GenericJob j)
{
	m_job = j;
}

JobCounter::JobCounter(JobFunction jf, JobBase * jd)
{
	m_job = { jf, jd };
}

JobCounter::~JobCounter() {
	m_job.m_task(m_job.m_data);
}
