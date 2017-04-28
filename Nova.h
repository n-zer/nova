#pragma once

#include <vector>
#include <array>
#include "Envelope.h"
#include "Globals.h"
#include "Job.h"
#include "WorkerThread.h"
#include "blockingconcurrentqueue.h"

namespace Nova {
	//Queues a set of Runnables
	template<typename ... Runnables>
	static void Push(Runnables&... runnables) {
		std::array<Envelope, sizeof...(Runnables) - internal::BatchCount<Runnables...>::value> envs;
		std::vector<Envelope> batchEnvs;
		batchEnvs.reserve(internal::BatchCount<Runnables...>::value * 4);
		internal::PackRunnable(envs, batchEnvs, runnables...);
		Push(envs);
		Push(batchEnvs);
	}

	//Queues a set of Runnables, invoking the envelope when all have finished
	template<typename ... Runnables>
	static void Push(Envelope & next, Runnables&... runnables) {
		internal::SealedEnvelope se(next);
		std::array<Envelope, sizeof...(runnables) - internal::BatchCount<Runnables...>::value> envs;
		std::vector<Envelope> batchEnvs;
		batchEnvs.reserve(internal::BatchCount<Runnables...>::value * 4);
		internal::PackRunnable(envs, batchEnvs, runnables...);
		for (Envelope & e : envs)
			e.AddSealedEnvelope(se);
		for (Envelope & e : batchEnvs)
			e.AddSealedEnvelope(se);
		Push(envs);
		Push(batchEnvs);
	}
	
	//Queues an envelope
	void Push(Envelope& e);

	//Queues an envelope (rvalue)
	void Push(Envelope&& e);

	//Queues a vector of envelopes
	void Push(std::vector<Envelope> & envs);

	template<unsigned N>
	void Push(std::array<Envelope, N> & envs) {
		internal::Resources::m_queue.enqueue_bulk(envs.begin(), envs.size());
	}

	//Loads a set of Runnables, queues them, then pauses the current call stack until they finish
	template<typename ... Runnables>
	static void Call(Runnables&... runnables) {
		std::vector<Envelope> envs;
		internal::PackRunnableNoAlloc(envs, runnables...);
		internal::Call(envs);
	}

	//Loads a vector of Runnables, queues them, then pauses the current call stack until they finish
	template<typename Runnable>
	static void Call(std::vector<Runnable> & runnables) {
		std::vector<Envelope> envs;
		envs.reserve(runnables.size());
		for (Runnable & r : runnables)
			internal::PackRunnableNoAlloc(envs, r);
		internal::Call(envs);
	}

	//Invokes a Callable object once for each value between start (inclusive) and end (exclusive), passing the value to each invocation
	template<typename Callable, typename ... Params>
	static void ParallelFor(Callable callable, BatchIndex start, BatchIndex end, Params... args) {
		Call(MakeBatchJob([&](BatchIndex start, BatchIndex end, Params... args) {
			for (BatchIndex c = start; c < end; c++)
				callable(c, args...);
		}, start, end, args...));
	}

	namespace internal{
		class Resources {
		public:
			static moodycamel::BlockingConcurrentQueue<Envelope> m_queue;
			static thread_local std::vector<LPVOID> m_availableFibers;
			static thread_local std::vector<Envelope> m_currentJobs;
		};

		//Breaks a Runnable off the parameter pack and recurses
		template<typename ... Runnables, unsigned int N>
		static void PackRunnable(std::array<Envelope, N> & envs, std::vector<Envelope> & batchEnvs, Runnables&... runnables) {
			unsigned int index(0);
			internal::PackRunnable(envs, index, batchEnvs, runnables...);
		}

		//Breaks a Runnable off the parameter pack and recurses
		template<typename Runnable, typename ... Runnables, unsigned int N>
		static void PackRunnable(std::array<Envelope, N> & envs, unsigned & index, std::vector<Envelope> & batchEnvs, Runnable & runnable, Runnables&... runnables) {
			internal::PackRunnable(envs, index, batchEnvs, runnable);
			internal::PackRunnable(envs, index, batchEnvs, runnables...);
		}

		//Loads a Runnable into an envelope and pushes it to the given vector
		template<typename Runnable, unsigned int N>
		static void PackRunnable(std::array<Envelope, N> & envs, unsigned & index, std::vector<Envelope> & batchEnvs, Runnable& runnable) {
			envs[index++] = { runnable };
		}

		//Special overload for batch jobs - splits into envelopes and inserts them into the given vector
		template<typename Callable, typename ... Params, unsigned int N>
		static void PackRunnable(std::array<Envelope, N> & envs, unsigned & index, std::vector<Envelope> & batchEnvs, BatchJob<Callable, Params...> & j) {
			std::vector<Envelope> splitEnvs = SplitBatchJob(j);
			envs.insert(envs.end(), splitEnvs.begin(), splitEnvs.end());
		}

		//Breaks a Runnable off the parameter pack and recurses
		template<typename Runnable, typename ... Runnables>
		static void PackRunnableNoAlloc(std::vector<Envelope> & envs, Runnable & runnable, Runnables&... runnables) {
			PackRunnableNoAlloc(envs, runnable);
			PackRunnableNoAlloc(envs, runnables...);
		}

		//Loads a Runnable into an envelope and pushes it to the given vector
		template<typename Runnable>
		static void PackRunnableNoAlloc(std::vector<Envelope> & envs, Runnable& runnable) {
			envs.emplace_back(&runnable);
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

		//Converts a BatchJob into a vector of Envelopes without copying the job to the heap
		template<typename Callable, typename ... Params>
		static std::vector<Envelope> SplitBatchJobNoAlloc(BatchJob<Callable, Params...> & j) {
			std::vector<Envelope> jobs;
			jobs.reserve(j.GetSections());
			for (unsigned int section = 0; section < j.GetSections(); section++) {
				jobs.emplace_back(&j);
			}
			return jobs;
		}

		template<typename... Args> struct BatchCount;

		template<>
		struct BatchCount<> {
			static const int value = 0;
		};

		template<typename ... Params, typename... Args>
		struct BatchCount<BatchJob<Params...>, Args...> {
			static const int value = 1 + BatchCount<Args...>::value;
		};

		template<typename First, typename... Args>
		struct BatchCount<First, Args...> {
			static const int value = BatchCount<Args...>::value;
		};

		//Attempts to grab an envelope from the queue
		void Pop(Envelope &e);

		void Call(std::vector<Envelope> & envs);

		void FinishCalledJob(LPVOID);

		//Starting point for a new fiber, queues a list of jobs and immediately enters the job loop.
		//This is used by the Call- functions to delay queueing of jobs until after the calling fiber
		//has been suspended.
		void QueueJobsAndEnterJobLoop(LPVOID jobPtr);
	}
}
