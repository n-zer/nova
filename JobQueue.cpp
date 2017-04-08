#include "JobQueue.h"

unsigned int JobQueuePool::m_size = 0;
vector<JobQueue> JobQueuePool::m_queues;

JobQueue::JobQueue()
{
	InitializeCriticalSection(&m_lock);
}

JobQueue::JobQueue(const JobQueue & other)
{
	m_jobs = other.m_jobs;
}

bool JobQueue::PopJob(Envelope &e)
{
	CriticalLock clock(m_lock);
	if (m_jobs.size() > 0) {
		e = m_jobs[0];
		m_jobs.pop_front();
		return true;
	}
	return false;
}

void JobQueue::PushJob(Envelope e)
{
	CriticalLock clock(m_lock);
	m_jobs.push_back(e);
}

void JobQueuePool::PushJob(Envelope&& e) {
	PushJob(e);
}

//Take a job and push it to neighboring queue
void JobQueuePool::PushJob(Envelope& e) {
	m_queues[(WorkerThread::GetThreadId() + 1) % m_size].PushJob(e);
}

bool JobQueuePool::PopJob(Envelope &e) {
	return m_queues[WorkerThread::GetThreadId()].PopJob(e);
}


void JobQueuePool::CallJob(Envelope e)
{

}

void JobQueuePool::CallBatchJob(Envelope e)
{

}

void JobQueuePool::QueueJobsAndEnterJobLoop(LPVOID jobPtr)
{

}

void JobQueuePool::InitPool(unsigned int numCores) {
	m_size = numCores;
	m_queues.resize(numCores);
}
