#pragma once

#include <vector>
#include "Envelope.h"
#include "Globals.h"
#include "Job.h"
#include "WorkerThread.h"
#include "blockingconcurrentqueue.h"

namespace Nova {
	//Builds and queues a standalone job
	template<typename Callable, typename ... Params>
	static void Push(Callable callable, Params... args) {
		Push(MakeJob(callable, args...));
	}

	//Queues a pre-built standalone job
	template<typename Callable, typename ... Params>
	static void Push(internal::SimpleJob<Callable, Params...> & j) {
		auto* basePtr = new internal::SimpleJob<Callable, Params...>(j);
		Envelope env(&Envelope::RunAndDeleteRunnable<internal::SimpleJob<Callable, Params...>>, basePtr);
		Push(env);
	}

	//Queues a pre-built standalone job (rvalue)
	template<typename Callable, typename ... Params>
	static void Push(internal::SimpleJob<Callable, Params...> && j) {
		Push(j);
	}

	//Builds and queues a batch job
	template<typename Callable, typename ... Params>
	static void PushBatch(Callable callable, Params... args) {
		Push(MakeBatchJob(callable, args...));
	}

	//Queues a pre-built batch job
	template<typename Callable, typename ... Params>
	static void Push(internal::BatchJob<Callable, Params...> & j) {
		Push(SplitBatchJob(j));
	}

	//Queues a pre-built batch job with a custom sealed envelope
	template<typename Callable, typename ... Params>
	static void Push(internal::BatchJob<Callable, Params...> & j, Envelope & next) {
		internal::SealedEnvelope se(next);
		std::vector<Envelope> jobs = SplitBatchJob(j);

		for (Envelope & e : jobs)
			e.AddSealedEnvelope(se);

		Push(jobs);
	}

	//Queues a pre-built batch job (rvalue)
	template<typename Callable, typename ... Params>
	static void Push(internal::BatchJob<Callable, Params...> && j) {
		Push(j);
	}

	//Queues a pre-built batch job (rvalue) with a custom sealed envelope
	template<typename Callable, typename ... Params>
	static void Push(internal::BatchJob<Callable, Params...> && j, Envelope & next) {
		Push(j, next);
	}

	//Queues a set of Runnables
	template<typename ... Runnables>
	static void Push(Runnables&... runnables) {
		std::vector<Envelope> envs;
		internal::PackRunnable(envs, runnables...);
		Push(envs);
	}

	//Queues a set of Runnables (rvalues)
	template<typename ... Runnables>
	static void Push(Runnables&&... runnables) {
		Push(runnables...);
	}

	//Queues a set of Runnables
	template<typename ... Runnables>
	static void Push(Runnables&... runnables, Envelope & next) {
		internal::SealedEnvelope se(next);
		std::vector<Envelope> envs;
		internal::PackRunnable(envs, runnables...);
		for (Envelope & e : envs)
			e.AddSealedEnvelope(se);
		Push(envs);
	}

	//Queues a set of Runnables (rvalues)
	template<typename ... Runnables>
	static void Push(Runnables&&... runnables, Envelope & next) {
		Push(runnables..., next);
	}

	template<typename Runnable>
	static void Push(std::vector<Runnable> & runnables) {
		std::vector<Envelope> envs;
		envs.reserve(runnables.size());
		for (Runnable & r : runnables)
			internal::PackRunnable(envs, r);
		Push(envs);
	}

	template<typename Runnable>
	static void Push(std::vector<Runnable> & runnables, Envelope & next) {
		internal::SealedEnvelope se(next);
		std::vector<Envelope> envs;
		envs.reserve(runnables.size());
		for (Runnable & r : runnables)
			internal::PackRunnable(envs, r);
		for (Envelope & e : envs)
			e.AddSealedEnvelope(se);
		Push(envs);
	}
	
	//Pushes an envelope to the calling thread's neighbor's queue
	void Push(Envelope& e);

	//Pushes a vector of envelopes across the pool
	void Push(std::vector<Envelope> & envs);

	//Loads a set of runnables, queues them across the pool, then pauses the current call stack until they finish
	template<typename ... Runnables>
	static void Call(Runnables&... runnables) {
		std::vector<Envelope> envs;
		internal::PackRunnableNoAlloc(envs, runnables...);
		internal::Call(envs);
	}

	template<typename Runnable>
	static void Call(std::vector<Runnable> & runnables) {
		std::vector<Envelope> envs;
		envs.reserve(runnables.size());
		for (Runnable & r : runnables)
			internal::PackRunnableNoAlloc(envs, r);
		internal::Call(envs);
	}

	template<typename Callable, typename ... Params>
	static void ParallelFor(Callable callable, unsigned start, unsigned end, Params... args) {
		Call(MakeBatchJob([&](BatchIndex start, BatchIndex end, Params... args) {
			for (BatchIndex c = start; c < end; c++)
				callable(c, args...);
		}, start, end, args...));
	}

	namespace internal{
		//Breaks a runnable off the parameter pack and recurses
		template<typename Runnable, typename ... Runnables>
		static void PackRunnable(std::vector<Envelope> & envs, Runnable & runnable, Runnables&... runnables) {
			internal::PackRunnable(envs, runnable);
			internal::PackRunnable(envs, runnables...);
		}

		//Loads a runnable into an envelope and pushes it to the given vector
		template<typename Runnable>
		static void PackRunnable(std::vector<Envelope> & envs, Runnable& runnable) {
			auto* basePtr = new Runnable(runnable);
			envs.emplace_back(&Envelope::RunAndDeleteRunnable<Runnable>, basePtr);
		}

		//Special overload for batch jobs - splits into envelopes and inserts them into the given vector
		template<typename Callable, typename ... Params>
		static void PackRunnable(std::vector<Envelope> & envs, BatchJob<Callable, Params...> & j) {
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
			envs.emplace_back(&Envelope::RunRunnable<Runnable>, &runnable);
		}

		//Special overload for batch jobs - splits into envelopes and inserts them into the given vector
		template<typename Callable, typename ... Params>
		static void PackRunnableNoAlloc(std::vector<Envelope> & envs, BatchJob<Callable, Params...> & j) {
			std::vector<Envelope> splitEnvs = SplitBatchJobNoAlloc(j);
			envs.insert(envs.end(), splitEnvs.begin(), splitEnvs.end());
		}

		//Converts a BatchJob into a vector of Envelopes
		template<typename Callable, typename ... Params>
		static std::vector<Envelope> SplitBatchJob(BatchJob<Callable, Params...> & j) {
			std::vector<Envelope> jobs;
			jobs.reserve(j.GetSections());
			auto* basePtr = new BatchJob<Callable, Params...>(j);
			SealedEnvelope se(Envelope(&Envelope::DeleteRunnable<BatchJob<Callable, Params...>>, basePtr));
			for (unsigned int section = 0; section < j.GetSections(); section++) {
				jobs.emplace_back(basePtr);
				jobs[section].AddSealedEnvelope(se);
			}
			return jobs;
		}

		//Converts a BatchJob into a vector of Envelopes
		template<typename Callable, typename ... Params>
		static std::vector<Envelope> SplitBatchJobNoAlloc(BatchJob<Callable, Params...> & j) {
			std::vector<Envelope> jobs;
			jobs.reserve(j.GetSections());
			for (unsigned int section = 0; section < j.GetSections(); section++) {
				jobs.emplace_back(&j);
			}
			return jobs;
		}

		//Attempts to grab an envelope from the calling thread's queue. Returns true if successful
		void Pop(Envelope &e);

		void Call(std::vector<Envelope> & envs);

		void FinishCalledJob(LPVOID);

		//Starting point for a new fiber, queues a list of jobs and immediately enters the job loop.
		//This is used by the Call- functions to delay queueing of jobs until after the calling fiber
		//has been suspended.
		void QueueJobsAndEnterJobLoop(LPVOID jobPtr);

		static moodycamel::BlockingConcurrentQueue<Envelope> m_queue;
		static thread_local std::vector<LPVOID> m_availableFibers;
		static thread_local std::vector<Envelope> m_currentJobs;
	}
}
