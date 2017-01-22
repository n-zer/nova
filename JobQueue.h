#pragma once

#include <deque>
#include <mutex>
#include <vector>
#include "Job.h"

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
	static void PushJob(Job j, unsigned int id) {
		m_queues[(id + 1) % m_size].PushJob(j);
	}
	static bool PopJob(Job &j, unsigned int id) {
		return m_queues[id].PopJob(j);
	}
	static void InitPool(unsigned int numCores) {
		m_size = numCores;
		m_queues = new JobQueue[numCores];
	}
private:
	static JobQueue* m_queues;
	static unsigned int m_size;
};
