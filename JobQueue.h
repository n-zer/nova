#pragma once

#include <deque>
#include <mutex>
#include <vector>
#include <Windows.h>
#include "GenericJob.h"
#include "Globals.h"
#include "JobCounter.h"
#include "JobFunction.h"

using namespace std;
class JobQueue {
public:
	JobQueue();
	JobQueue(const JobQueue &);
	deque<Envelope> m_jobs;

	//Attempts to remove a job from the queue. Returns true if successful
	bool PopJob(Envelope &j);

	//Pushes a job to the queue
	void PushJob(Envelope j);
private:
	mutex m_lock;
};

class JobQueuePool {
public:
	template<typename ... Ts>
	static void PushJob(Ts... args) {
		PushJob(MakeJob(args...));
	}


	template<typename Callable, typename ... Ts>
	static void PushJob(SimpleJob<Callable, Ts...> && j) {
		PushJob(j);
	}

	template<typename Callable, typename ... Ts>
	static void PushJob(SimpleJob<Callable, Ts...> & j) {
		void* basePtr = new SimpleJob<Callable, Ts...>(j);
		Envelope env(&RunRunnable<SimpleJob<Callable, Ts...>>, basePtr);
		env.AddSealedEnvelope({ { &DeleteRunnable<SimpleJob<Callable,Ts...>>,basePtr } });
		PushJob(env);
	}

	//Pushes a job to the calling thread's neighbor's queue
	static void PushJob(Envelope&& e);

	//Pushes a job to the calling thread's neighbor's queue
	static void PushJob(Envelope& e);

	//Attempts to grab a job from the calling thread's queue. Returns true if successful
	static bool PopJob(Envelope &e);


	//Takes a job with a given range (e.g. 5 - 786) and splits it into a number of jobs
	//equal or less than the number of worker threads, with each resulting job having
	//a contiguous portion of the initial range. Useful for implementing parallel_for-esque
	//functionality.
	template<typename ... Ts>
	static void PushJobAsBatch(Job<unsigned int, unsigned int, Ts...> & j) {
		unsigned int start = std::get<0>(j.m_tuple);
		unsigned int end = std::get<1>(j.m_tuple);
		unsigned int count = start - end;

		std::vector<GenericJob> jobs;

		unsigned int sections = m_size;
		if (count < sections)
			sections = count;
		JobBase* basePtr = new BatchWrapper<Job<unsigned int, unsigned int, Ts...>>(j, (WorkerThread::GetThreadId() + 1) % m_size, sections);
		auto jc = std::make_shared<JobCounter>(&GenericJob::DeleteData, basePtr);
		for (unsigned int section = 1; section <= sections; section++) {
			GenericJob gj(&GenericJob::RunBatchJob<Ts...>, basePtr);
			gj.m_counters.push_back(jc);
			jobs.push_back(gj);
		}

		unsigned int id = WorkerThread::GetThreadId();
		for (unsigned int c = 0; c < jobs.size(); c++) {
			m_queues[(id + c) % m_size].PushJob(jobs[c]);
		}
	}

	//Creates a child job. Pushes the new job then suspends the current fiber. When the
	//child job is completed the current thread will switch back to the suspended fiber.
	static void CallJob(GenericJob j);

	//Creates a child batch job. Pushes the new jobs then suspends the current fiber. When all the
	//resulting jobs are completed the current thread will switch back to the suspended fiber.
	static void CallBatchJob(GenericJob j);

	static void InitPool(unsigned int numCores);

	//Starting point for a new fiber, queues a list of jobs and immediately enters the job loop.
	//This is used by the Call- functions to delay queueing of jobs until after the calling fiber
	//has been suspended.
	static void QueueJobsAndEnterJobLoop(LPVOID jobPtr);
private:
	static vector<JobQueue> m_queues;
	static unsigned int m_size;
};
