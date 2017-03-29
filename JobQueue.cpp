#include "JobQueue.h"
#include "WorkerThread.h"
#include "Job.h"

unsigned int JobQueuePool::m_size = 0;
vector<JobQueue> JobQueuePool::m_queues;

JobQueue::JobQueue()
{
}

JobQueue::JobQueue(const JobQueue & other)
{
	m_jobs = other.m_jobs;
}

bool JobQueue::PopJob(GenericJob &j)
{
	lock_guard<mutex> lg(m_lock);
	if (m_jobs.size() > 0) {
		j = m_jobs[0];
		m_jobs.pop_front();
		return true;
	}
	return false;
}

void JobQueue::PushJob(GenericJob j)
{
	lock_guard<mutex> lg(m_lock);
	m_jobs.push_back(j);
}

//Take a job, add a counter to it that deletes its data, then push it to neighboring queue
void JobQueuePool::PushJob(GenericJob& j) {
	j.m_counters.push_back(std::make_shared<JobCounter>(&GenericJob::DeleteData, j.m_data));
	m_queues[(WorkerThread::GetThreadId() + 1) % m_size].PushJob(j);
}

bool JobQueuePool::PopJob(GenericJob &j) {
	return m_queues[WorkerThread::GetThreadId()].PopJob(j);
}

//void JobQueuePool::PushJobAsBatch(GenericJob& j, unsigned int start, unsigned int end)
//{
//	//calculate number of items per core
//	unsigned int lengthPer = static_cast<int>(ceilf(static_cast<float>(end - start) / m_size));
//	unsigned int numJobs = 0;
//
//	//create counter to delete underlying data
//	shared_ptr<JobCounter> jc = std::make_shared<JobCounter>(&GenericJob::DeleteBatchData, bjd.data);
//
//	//create per-core jobs (there may be less than one per core)
//	vector<GenericJob> bjs(m_size);
//	for (unsigned int c = 0; c < m_size; c++) {
//		//clamp remaining items to number of items per core
//		unsigned int len = clip(end - start, unsigned int(0), lengthPer);
//		bjs[c].m_task = j.m_task;
//		bjs[c].m_data = new BatchJobData(bjd.start, len, bjd.data);
//		bjs[c].m_counters = j.m_counters;
//		bjs[c].m_counters.push_back(jc);
//		//move starting point forwards
//		if (len > 0) {
//			bjd.start += lengthPer;
//			numJobs++;
//		}
//	}
//
//	//push jobs to queues
//	unsigned int id = WorkerThread::GetThreadId();
//	for (unsigned int c = 0; c < numJobs; c++) {
//		m_queues[(id + c) % m_size].PushJob(bjs[c]);
//	}
//}

void JobQueuePool::CallJob(GenericJob j)
{

}

void JobQueuePool::CallBatchJob(GenericJob j)
{

}

void JobQueuePool::InitPool(unsigned int numCores) {
	m_size = numCores;
	m_queues.resize(numCores);
}

void JobQueuePool::QueueJobsAndEnterJobLoop(LPVOID jobPtr)
{
	
}
