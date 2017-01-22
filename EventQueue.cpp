#include "JobQueue.h"

unsigned int JobQueuePool::m_size = 0;
JobQueue* JobQueuePool::m_queues = nullptr;

bool JobQueue::PopJob(Job &j)
{
	lock_guard<mutex> lg(m_lock);
	if (m_jobs.size() > 0) {
		j = m_jobs[0];
		m_jobs.pop_front();
		return true;
	}
	return false;
}

void JobQueue::PushJob(Job j)
{
	lock_guard<mutex> lg(m_lock);
	m_jobs.push_back(j);
}
