#include "JobCounter.h"

JobCounter::JobCounter()
{
	m_function = nullptr;
}

JobCounter::JobCounter(PushFunction pf, Job j, unsigned int limit)
{
	m_function = pf;
	m_job = j;
	m_limit = limit;
}

void JobCounter::Init(PushFunction pf, Job j, unsigned int limit)
{
	m_function = pf;
	m_job = j;
	m_limit = limit;
}

void JobCounter::Increment(unsigned int i)
{
	m_counter += i;
	if (m_counter.load() >= m_limit && m_function != nullptr) {
		m_counter = 0;
		m_function(m_job);
	}
}
