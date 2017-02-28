#include "JobCounter.h"
#include "Job.h"
#include "JobData.h"

JobCounter::JobCounter(Job j)
{
	m_job = j;
}

JobCounter::JobCounter(JobFunction jf, JobData * jd)
{
	m_job = { jf, jd };
}

JobCounter::~JobCounter() {
	m_job.m_task(m_job.m_data);
}
