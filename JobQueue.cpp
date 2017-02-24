#include "JobQueue.h"
#include "WorkerThread.h"
#include "JobData.h"

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
	//get job data
	BatchJobData & bjd = *static_cast<BatchJobData*>(j.m_data);

	//calculate number of items per core
	unsigned int lengthPer = static_cast<int>(ceilf(static_cast<float>(bjd.count) / m_size));
	unsigned int end = bjd.start + bjd.count;
	unsigned int numJobs = 0;

	//create per-core jobs (there may be less than one per core)
	vector<BatchJobData> bjds(m_size);
	for (unsigned int c = 0; c < m_size; c++) {
		//clamp remaining items to number of items per core
		unsigned int len = clip(end - bjd.start, unsigned int(0), lengthPer);
		bjds[c].start = bjd.start;
		bjds[c].count = len;
		bjds[c].jobData = bjd.jobData;
		//move starting point forwards
		if (len > 0) {
			bjd.start += lengthPer;
			numJobs++;
		}
	}

	//push jobs to queues
	unsigned int id = WorkerThread::GetThreadId();
	for (unsigned int c = 0; c < numJobs; c++) {
		BatchJobData* newBjd = new BatchJobData(bjds[c]); //slicing, fix this
		m_queues[(id + c) % m_size].PushJob({ j.m_task,newBjd });
	}
}

void JobQueuePool::CallJob(Job j)
{

}

void JobQueuePool::CallBatchJob(Job j)
{

}

void JobQueuePool::InitPool(unsigned int numCores) {
	m_size = numCores;
	m_queues.resize(numCores);
}


void JobQueuePool::QueueJobsAndEnterJobLoop(LPVOID jobPtr)
{
	
}
