#pragma once

#include <deque>
#include <mutex>
#include <vector>
#include <Windows.h>
#include "Job.h"
#include "Globals.h"

using namespace std;
class JobQueue {
public:
	JobQueue();
	JobQueue(const JobQueue &);
	deque<Job> m_jobs;

	//Attempts to remove a job from the queue. Returns true if successful
	bool PopJob(Job &j);

	//Pushes a job to the queue
	void PushJob(Job j);
private:
	mutex m_lock;
};

class JobQueuePool {
public:
	//Pushes a job to the calling thread's neighbor's queue
	static void PushJob(Job& j);

	//Attempts to grab a job from the calling thread's queue. Returns true if successful
	static bool PopJob(Job &j);

	//Takes a job with a given range (e.g. 5 - 786) and splits it into a number of jobs
	//equal or less than the number of worker threads, with each resulting job having
	//a contiguous portion of the initial range. Useful for implementing parallel_for-esque
	//functionality.
	static void PushBatchJob(Job& j);

	//Creates a child job. Pushes the new job then suspends the current fiber. When the
	//child job is completed the current thread will switch back to the suspended fiber.
	static void CallJob(Job j);

	//Creates a child batch job. Pushes the new jobs then suspends the current fiber. When all the
	//resulting jobs are completed the current thread will switch back to the suspended fiber.
	static void CallBatchJob(Job j);

	static void InitPool(unsigned int numCores);

	//Starting point for a new fiber, queues a list of jobs and immediately enters the job loop.
	//This is used by the Call- functions to delay queueing of jobs until after the calling fiber
	//has been suspended.
	static void QueueJobsAndEnterJobLoop(LPVOID jobPtr);
private:
	static vector<JobQueue> m_queues;
	static unsigned int m_size;
};
