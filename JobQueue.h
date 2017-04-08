#pragma once

#include <deque>
#include <vector>
#include <Windows.h>
#include "Envelope.h"
#include "Globals.h"
#include "WorkerThread.h"
#include "Job.h"
#include "CriticalLock.h"

using namespace std;
class JobQueue {
public:
	JobQueue();
	JobQueue(const JobQueue &);
	deque<Envelope> m_jobs;

	//Attempts to remove a job from the queue. Returns true if successful
	void PopJob(Envelope &j);

	//Pushes a job to the queue
	void PushJob(Envelope j);
private:
	CRITICAL_SECTION m_lock;
	CONDITION_VARIABLE m_cv;
};

class JobQueuePool {
public:
	//Builds and queues a standalone job
	template<typename Callable, typename ... Ts>
	static void PushJob(Callable callable, Ts... args) {
		PushJob(MakeJob(callable, args...));
	}

	//Queues a pre-built standalone job
	template<typename Callable, typename ... Ts>
	static void PushJob(SimpleJob<Callable, Ts...> & j) {
		void* basePtr = new SimpleJob<Callable, Ts...>(j);
		Envelope env(&RunRunnable<SimpleJob<Callable, Ts...>>, basePtr);
		env.AddSealedEnvelope({ { &DeleteRunnable<SimpleJob<Callable,Ts...>>,basePtr } });
		PushJob(env);
	}

	//Queues a pre-built standalone job (rvalue)
	template<typename Callable, typename ... Ts>
	static void PushJob(SimpleJob<Callable, Ts...> && j) {
		PushJob(j);
	}

	//Builds and queues a batch job
	template<typename Callable, typename ... Ts>
	static void PushJobAsBatch(Callable callable, unsigned start, unsigned end, Ts... args) {
		PushJobAsBatch(MakeBatchJob(callable, start, end, args...));
	}

	//Queues a pre-built batch job
	template<typename Callable, typename ... Ts>
	static void PushJobAsBatch(BatchJob<Callable, Ts...> & j) {
		std::vector<Envelope> jobs;

		auto* basePtr = new BatchJob<Callable, Ts...>(j);
		SealedEnvelope se({ &DeleteRunnable<BatchJob<Callable,Ts...>>,basePtr });
		for (unsigned int section = 1; section <= j.GetSections(); section++) {
			Envelope gj(&RunRunnable<BatchJob<Callable, Ts...>>, basePtr);
			gj.AddSealedEnvelope(se);
			jobs.push_back(gj);
		}

		unsigned int id = WorkerThread::GetThreadId();
		for (unsigned int c = 0; c < jobs.size(); c++) {
			m_queues[(id + c) % m_size].PushJob(jobs[c]);
		}
	}

	//Queues a pre-built batch job (rvalue)
	template<typename Callable, typename ... Ts>
	static void PushJobAsBatch(BatchJob<Callable, Ts...> && j) {
		PushJobAsBatch(j);
	}

	//Pushes an envelope to the calling thread's neighbor's queue
	static void PushJob(Envelope&& e);

	//Pushes an envelope to the calling thread's neighbor's queue
	static void PushJob(Envelope& e);

	//Attempts to grab an envelope from the calling thread's queue. Returns true if successful
	static void PopJob(Envelope &e);

	//Creates a child job. Pushes the new job then suspends the current fiber. When the
	//child job is completed the current thread will switch back to the suspended fiber.
	static void CallJob(Envelope e);

	//Creates a child batch job. Pushes the new jobs then suspends the current fiber. When all the
	//resulting jobs are completed the current thread will switch back to the suspended fiber.
	static void CallBatchJob(Envelope e);

	static void InitPool(unsigned int numCores);

	//Starting point for a new fiber, queues a list of jobs and immediately enters the job loop.
	//This is used by the Call- functions to delay queueing of jobs until after the calling fiber
	//has been suspended.
	static void QueueJobsAndEnterJobLoop(LPVOID jobPtr);
private:
	static vector<JobQueue> m_queues;
	static unsigned int m_size;
};
