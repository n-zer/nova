#include "JobCounter.h"
#include "Job.h"

JobCounter::JobCounter()
{
	m_function = nullptr;
}

JobCounter::JobCounter(PushFunction pf, Job j)
{
	m_function = pf;
	m_job = j;
}

JobCounter::~JobCounter() {
	if (m_function)
		m_function(m_job);
}
