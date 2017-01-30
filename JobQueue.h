#pragma once

#include <deque>
#include <mutex>
#include <vector>
#include "Job.h"
#include "WorkerThread.h"
#include "BatchJobData.h"
#include "Globals.h"

typedef void (*PushFunction)(Job);

using namespace std;
class JobQueue {
public:
	JobQueue();
	JobQueue(const JobQueue &);
	deque<Job> m_jobs;
	bool PopJob(Job &j);
	void PushJob(Job j);
private:
	mutex m_lock;
};

class JobQueuePool {
public:
	static void PushJob(Job j);
	static bool PopJob(Job &j);
	static void PushBatchJob(Job j);
	static void InitPool(unsigned int numCores);
private:
	static vector<JobQueue> m_queues;
	static unsigned int m_size;
};
