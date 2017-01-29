#include "JobQueue.h"

unsigned int JobQueuePool::m_size = 0;
vector<JobQueue> JobQueuePool::m_queues;

JobQueue::JobQueue()
{
}

JobQueue::JobQueue(const JobQueue & other)
{
	m_jobs = other.m_jobs;
}

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

void JobQueuePool::PushJob(Job j) {
	m_queues[(WorkerThread::GetThreadId() + 1) % m_size].PushJob(j);
}

bool JobQueuePool::PopJob(Job &j) {
	return m_queues[WorkerThread::GetThreadId()].PopJob(j);
}

void JobQueuePool::InitPool(unsigned int numCores) {
	m_size = numCores;
	m_queues.resize(numCores);
}