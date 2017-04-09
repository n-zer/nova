#include "JobQueue.h"

unsigned int JobQueuePool::m_size = 0;
vector<JobQueue> JobQueuePool::m_queues;

JobQueue::JobQueue()
{
	InitializeCriticalSection(&m_lock);
	InitializeConditionVariable(&m_cv);
}

JobQueue::JobQueue(const JobQueue & other)
{
	m_jobs = other.m_jobs;
}

void JobQueue::PopJob(Envelope &e)
{
	CriticalLock clock(m_lock);

	while (m_jobs.size() == 0) {
		SleepConditionVariableCS(&m_cv, &m_lock, INFINITE);
	}

	e = m_jobs[0];
	m_jobs.pop_front();
}

void JobQueue::PushJob(Envelope e)
{
	{
		CriticalLock clock(m_lock);
		m_jobs.push_back(e);
	}
	WakeConditionVariable(&m_cv);
}

void JobQueuePool::PushJob(Envelope&& e) {
	PushJob(e);
}

//Take a job and push it to neighboring queue
void JobQueuePool::PushJob(Envelope& e) {
	m_queues[WorkerThread::GetThreadId() % m_size].PushJob(e);
}

void JobQueuePool::PushJobs(std::vector<Envelope>& envs)
{
	unsigned id = WorkerThread::GetThreadId();
	for (unsigned c = 0; c < envs.size(); c++)
		m_queues[(id + c + 1) % m_size].PushJob(envs[c]);
}

void JobQueuePool::PopJob(Envelope &e) {
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
