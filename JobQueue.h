#pragma once

#include <deque>
#include <vector>
#include <Windows.h>
#include "Envelope.h"
#include "Globals.h"
#include "Job.h"
#include "blockingconcurrentqueue.h"

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
		auto* basePtr = new SimpleJob<Callable, Ts...>(j);
		Envelope env(&Envelope::RunAndDeleteRunnable<SimpleJob<Callable, Ts...>>, basePtr);
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
		PushJobs(SplitBatchJob(j));
	}

	//Queues a pre-built batch job with a custom sealed envelope
	template<typename Callable, typename ... Ts>
	static void PushJobAsBatch(BatchJob<Callable, Ts...> & j, SealedEnvelope next) {
		std::vector<Envelope> jobs = SplitBatchJob(j);

		for (unsigned c = 0; c < jobs.size(); c++)
			jobs[c].AddSealedEnvelope(next);

		PushJobs(jobs);
	}

	//Queues a pre-built batch job (rvalue)
	template<typename Callable, typename ... Ts>
	static void PushJobAsBatch(BatchJob<Callable, Ts...> && j) {
		PushJobAsBatch(j);
	}

	//Queues a pre-built batch job (rvalue) with a custom sealed envelope
	template<typename Callable, typename ... Ts>
	static void PushJobAsBatch(BatchJob<Callable, Ts...> && j, SealedEnvelope next) {
		PushJobAsBatch(j, next);
	}

	//Loads a set of runnables into envelopes and queues them across the pool
	template<typename ... Runnables>
	static void PushJobs(Runnables&... runnables) {
		std::vector<Envelope> envs;
		PackRunnable(envs, runnables...);
		PushJobs(envs);
	}

	//Loads a set of runnables (rvalues) into envelopes and queues them across the pool
	template<typename ... Runnables>
	static void PushJobs(Runnables&&... runnables) {
		PushJobs(runnables...);
	}

	//Breaks a runnable off the parameter pack and recurses
	template<typename Runnable, typename ... Runnables>
	static void PackRunnable(std::vector<Envelope> & envs, Runnable & runnable, Runnables&... runnables) {
		PackRunnable(envs, runnable);
		PackRunnable(envs, runnables...);
	}

	//Loads a runnable into an envelope and pushes it to the given vector
	template<typename Runnable>
	static void PackRunnable(std::vector<Envelope> & envs, Runnable& runnable) {
		auto* basePtr = new Runnable(runnable);
		envs.push_back({ &Envelope::RunAndDeleteRunnable<Runnable>, basePtr });
	}

	//Special overload for batch jobs - splits into envelopes and inserts them into the given vector
	template<typename Callable, typename ... Ts>
	static void PackRunnable(std::vector<Envelope> & envs, BatchJob<Callable, Ts...> & j) {
		std::vector<Envelope> splitEnvs = SplitBatchJob(j);
		envs.insert(envs.end(), splitEnvs.begin(), splitEnvs.end());
	}

	//Breaks a runnable off the parameter pack and recurses
	template<typename Runnable, typename ... Runnables>
	static void PackRunnableNoAlloc(std::vector<Envelope> & envs, Runnable & runnable, Runnables&... runnables) {
		PackRunnableNoAlloc(envs, runnable);
		PackRunnableNoAlloc(envs, runnables...);
	}

	//Loads a runnable into an envelope and pushes it to the given vector
	template<typename Runnable>
	static void PackRunnableNoAlloc(std::vector<Envelope> & envs, Runnable& runnable) {
		envs.push_back({ &Envelope::RunRunnable<Runnable>, &runnable });
	}

	//Special overload for batch jobs - splits into envelopes and inserts them into the given vector
	template<typename Callable, typename ... Ts>
	static void PackRunnableNoAlloc(std::vector<Envelope> & envs, BatchJob<Callable, Ts...> & j) {
		std::vector<Envelope> splitEnvs = SplitBatchJobNoAlloc(j);
		envs.insert(envs.end(), splitEnvs.begin(), splitEnvs.end());
	}

	//Converts a BatchJob into a vector of Envelopes
	template<typename Callable, typename ... Ts>
	static std::vector<Envelope> SplitBatchJob(BatchJob<Callable, Ts...> & j) {
		std::vector<Envelope> jobs;
		auto* basePtr = new BatchJob<Callable, Ts...>(j);
		SealedEnvelope se(Envelope(&Envelope::DeleteRunnable<BatchJob<Callable, Ts...>>, basePtr));
		for (unsigned int section = 1; section <= j.GetSections(); section++) {
			Envelope gj(basePtr);
			gj.AddSealedEnvelope(se);
			jobs.push_back(gj);
		}
		return jobs;
	}

	//Converts a BatchJob into a vector of Envelopes
	template<typename Callable, typename ... Ts>
	static std::vector<Envelope> SplitBatchJobNoAlloc(BatchJob<Callable, Ts...> & j) {
		std::vector<Envelope> jobs;
		for (unsigned int section = 1; section <= j.GetSections(); section++) {
			Envelope gj(&j);
			jobs.push_back(gj);
		}
		return jobs;
	}

	//Pushes an envelope to the calling thread's neighbor's queue
	static void PushJob(Envelope&& e);

	//Pushes an envelope to the calling thread's neighbor's queue
	static void PushJob(Envelope& e);

	//Attempts to grab an envelope from the calling thread's queue. Returns true if successful
	static void PopJob(Envelope &e);

	//Pushes a vector of envelopes across the pool
	static void PushJobs(std::vector<Envelope> & envs);

	//Loads a set of runnables, queues them across the pool, then pauses the current call stack until they finish
	template<typename ... Runnables>
	static void CallJobs(Runnables&... runnables) {
		std::vector<Envelope> envs;
		PackRunnableNoAlloc(envs, runnables...);
		CallJobs(envs);
	}

	static void CallJobs(std::vector<Envelope> & e);

	static void FinishCalledJob(LPVOID);

	//Starting point for a new fiber, queues a list of jobs and immediately enters the job loop.
	//This is used by the Call- functions to delay queueing of jobs until after the calling fiber
	//has been suspended.
	static void QueueJobsAndEnterJobLoop(LPVOID jobPtr);

	template<typename Callable, typename ... Ts>
	static void ParallelFor(Callable callable, unsigned start, unsigned end, Ts... args) {
		CallJobs(MakeBatchJob([&](unsigned start, unsigned end, Ts... args) {
			for (unsigned c = start; c < end; c++)
				callable(c, args...);
		}, start, end, args...));
	}

private:
	static moodycamel::BlockingConcurrentQueue<Envelope> m_queue;
	static thread_local std::vector<LPVOID> m_availableFibers;
	static thread_local std::vector<Envelope> m_currentJobs;
};
