#pragma once

#include <deque>
#include <mutex>
#include <vector>
#include "Job.h"
#include "WorkerThread.h"

using namespace std;
class JobQueue {
public:
	deque<Job> m_jobs;
	bool PopJob(Job &j);
	void PushJob(Job j);
private:
	mutex m_lock;
};

class JobQueuePool {
public:
	static void PushJob(Job j) {
		m_queues[(WorkerThread::GetThreadId() + 1) % m_size].PushJob(j);
	}
	static bool PopJob(Job &j) {
		return m_queues[WorkerThread::GetThreadId()].PopJob(j);
	}
	static void InitPool(unsigned int numCores) {
		//m_size = numCores;
		//m_queues = new JobQueue[numCores];
	}
private:
	static vector<JobQueue> m_queues;
	static unsigned int m_size;
};
