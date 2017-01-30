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

void JobQueuePool::PushBatchJob(Job j)
{
	BatchJobData bjd = *static_cast<BatchJobData*>(j.m_data);
	unsigned int lengthPer = static_cast<int>(ceilf(static_cast<float>(bjd.count) / m_size));
	unsigned int end = bjd.start + bjd.count;
	unsigned int numJobs = 0;
	BatchJobData* bjds = new BatchJobData[m_size]();
	for (unsigned int c = 0; c < m_size; c++) {
		unsigned int len = clip(end - bjd.start, unsigned int(0), lengthPer);
		bjds[c].start = bjd.start;
		bjds[c].count = len;
		if (len > 0) {
			bjd.start += lengthPer;
			numJobs++;
		}
	}
	unsigned int id = WorkerThread::GetThreadId();
	for (unsigned int c = 0; c < numJobs; c++) {
		BatchJobData* newBjd = new BatchJobData();
		*newBjd = bjds[c];
		m_queues[(id + c) % m_size].PushJob({ j.m_task,newBjd });
	}
	delete[] bjds;
}

void JobQueuePool::InitPool(unsigned int numCores) {
	m_size = numCores;
	m_queues.resize(numCores);
}