#pragma once

#include <vector>
#include <array>
#include "Envelope.h"
#include "Globals.h"
#include "Job.h"
#include "WorkerThread.h"
#include "QueueWrapper.h"

namespace Nova {
	//Queues a set of Runnables
	template<bool ToMain = false, typename ... Runnables>
	static void Push(Runnables&&... runnables) {
		using namespace internal;
		std::array<Envelope, sizeof...(Runnables)-BatchCount<Runnables...>::value> envs;
		std::vector<Envelope> batchEnvs;
		batchEnvs.reserve(BatchCount<Runnables...>::value * 4);
		PackRunnable(envs, batchEnvs, std::forward<Runnables...>(runnables...));
		Push<ToMain>(std::move(envs));
		Push<ToMain>(std::move(batchEnvs));
	}

	//Queues a set of Runnables, invoking the envelope when all have finished
	template<bool ToMain = false, typename ... Runnables>
	static void Push(SealedEnvelope & next, Runnables&&... runnables) {
		using namespace internal;
		std::array<Envelope, sizeof...(runnables)-BatchCount<Runnables...>::value> envs;
		std::vector<Envelope> batchEnvs;
		batchEnvs.reserve(internal::BatchCount<Runnables...>::value * 4);
		PackRunnable(envs, batchEnvs, std::forward<Runnables...>(runnables...));
		Push<ToMain>(next, std::move(envs));
		Push<ToMain>(next, std::move(batchEnvs));
	}

	//Loads a set of Runnables, queues them, then pauses the current call stack until they finish
	template<bool ToMain = false, bool FromMain = false, typename ... Runnables>
	static void Call(Runnables&... runnables) {
		using namespace internal;
		std::array<Envelope, sizeof...(Runnables) - BatchCount<Runnables...>::value> envs;
		std::vector<Envelope> batchEnvs;
		PackRunnableNoAlloc(envs, batchEnvs, runnables...);
		Call<ToMain, FromMain>(std::move(envs), std::move(batchEnvs));
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
			//Meyers singletons
			static QueueWrapper<Envelope>& QueueWrapper() {
				static Nova::internal::QueueWrapper<Nova::internal::Envelope> qw;
				return qw;
			}
			static std::vector<LPVOID>& AvailableFibers() {
				static thread_local std::vector<LPVOID> af;
				return af;
			}
			static SealedEnvelope *& CallTrigger() {
				static thread_local SealedEnvelope * ct;
				return ct;
			}
		};

		//Queues an envelope
		template<bool ToMain>
		void Push(internal::Envelope&& e) {
			internal::Resources::QueueWrapper().Push<ToMain>(std::forward<internal::Envelope>(e));
		}

		//Queues a vector of envelopes
		template<bool ToMain>
		void Push(std::vector<internal::Envelope> && envs) {
			internal::Resources::QueueWrapper().Push<ToMain>(std::forward<decltype(envs)>(envs));
		}

		template<bool ToMain, unsigned N>
		void Push(std::array<internal::Envelope, N> && envs) {
			internal::Resources::QueueWrapper().Push<ToMain>(std::forward<decltype(envs)>(envs));
		}

		//Generates Envelopes from the given Runnables and inserts them into a std::array (for standalone) or a std::vector (for batch)
		template<typename ... Runnables, unsigned int N>
		static void PackRunnable(std::array<Envelope, N> & envs, std::vector<Envelope> & batchEnvs, Runnables&&... runnables) {
			unsigned int index(0);
			PackRunnable(envs, index, batchEnvs, std::forward<Runnables...>(runnables...));
		}

		//Breaks a Runnable off the parameter pack and recurses
		template<typename Runnable, typename ... Runnables, unsigned int N>
		static void PackRunnable(std::array<Envelope, N> & envs, unsigned & index, std::vector<Envelope> & batchEnvs, Runnable && runnable, Runnables&&... runnables) {
			PackRunnable(envs, index, batchEnvs, std::forward<Runnable>(runnable));
			PackRunnable(envs, index, batchEnvs, std::forward<Runnables...>(runnables...));
		}

		//Loads a Runnable into an envelope and pushes it to the given vector
		template<typename Runnable, unsigned int N>
		static void PackRunnable(std::array<Envelope, N> & envs, unsigned & index, std::vector<Envelope> & batchEnvs, Runnable&& runnable) {
			envs[index++] = std::move(Envelope{ std::forward<Runnable>(runnable) });
		}

		//Special overload for batch jobs - splits into envelopes and inserts them into the given vector
		template<typename Callable, typename ... Params, unsigned int N>
		static void PackRunnable(std::array<Envelope, N> & envs, unsigned & index, std::vector<Envelope> & batchEnvs, BatchJob<Callable, Params...> && j) {
			std::vector<Envelope> splitEnvs = SplitBatchJob(std::forward<decltype(j)>(j));
			batchEnvs.insert(batchEnvs.end(), std::make_move_iterator(splitEnvs.begin()), std::make_move_iterator(splitEnvs.end()));
		}

		//Breaks a Runnable off the parameter pack and recurses
		template<typename ... Runnables, unsigned int N>
		static void PackRunnableNoAlloc(std::array<Envelope, N> & envs, std::vector<Envelope> & batchEnvs, Runnables&... runnables) {
			unsigned int index(0);
			PackRunnableNoAlloc(envs, index, batchEnvs, runnables...);
		}

		//Breaks a Runnable off the parameter pack and recurses
		template<typename Runnable, typename ... Runnables, unsigned int N>
		static void PackRunnableNoAlloc(std::array<Envelope, N> & envs, unsigned & index, std::vector<Envelope> & batchEnvs, Runnable & runnable, Runnables&... runnables) {
			PackRunnableNoAlloc(envs, index, batchEnvs, runnable);
			PackRunnableNoAlloc(envs, index, batchEnvs, runnables...);
		}

		//Loads a Runnable into an envelope and pushes it to the given vector
		template<typename Runnable, unsigned int N>
		static void PackRunnableNoAlloc(std::array<Envelope, N> & envs, unsigned & index, std::vector<Envelope> & batchEnvs, Runnable& runnable) {
			envs[index++] = { &runnable };
		}

		//Special overload for batch jobs - splits into envelopes and inserts them into the given vector
		template<typename Callable, typename ... Params, unsigned int N>
		static void PackRunnableNoAlloc(std::array<Envelope, N> & envs, unsigned & index, std::vector<Envelope> & batchEnvs, BatchJob<Callable, Params...> & j) {
			std::vector<Envelope> splitEnvs = SplitBatchJobNoAlloc(j);
			envs.insert(envs.end(), splitEnvs.begin(), splitEnvs.end());
		}

		//Converts a BatchJob into a vector of Envelopes
		template<typename Callable, typename ... Params>
		static std::vector<Envelope> SplitBatchJob(BatchJob<Callable, Params...> && j) {
			std::vector<Envelope> jobs;
			jobs.reserve(j.GetSections());
			auto* basePtr = new BatchJob<Callable, Params...>(std::forward<decltype(j)>(j));
			SealedEnvelope se(Envelope(&Envelope::NoOp, &Envelope::DeleteRunnable<BatchJob<Callable, Params...>>, basePtr));
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

		template<bool ToMain, unsigned N>
		void Push(SealedEnvelope & se, std::array<Envelope, N> && envs) {
			for (Envelope & e : envs)
				e.AddSealedEnvelope(se);
			Resources::QueueWrapper().Push<ToMain>(std::forward<decltype(envs)>(envs));
		}

		//Queues a vector of envelopes
		template<bool ToMain>
		void Push(SealedEnvelope & se, std::vector<Envelope> && envs) {
			for (Envelope & e : envs)
				e.AddSealedEnvelope(se);
			Resources::QueueWrapper().Push<ToMain>(std::forward<decltype(envs)>(envs));
		}

		//Attempts to grab an envelope from the queue
		inline void Pop(Envelope &e) {
			Resources::QueueWrapper().Pop(e);
		}

		inline void PopMain(Envelope &e) {
			Resources::QueueWrapper().PopMain(e);
		}

		template<bool ToMain, bool FromMain, unsigned int N>
		void Call(std::array<Envelope, N> && envs, std::vector<Envelope> && batchEnvs) {
			if (envs.size() == 0 && batchEnvs.size() == 0)
				throw std::runtime_error("Attempting to call no Envelopes");

			PVOID currentFiber = GetCurrentFiber();
			auto completionJob = [=]() {
				Nova::internal::Push<FromMain>(MakeJob(&FinishCalledJob, currentFiber));
			};

			SealedEnvelope se(Envelope{ &completionJob });
			Resources::CallTrigger() = &se;

			Push<ToMain>(se, std::forward<decltype(envs)>(envs));

			Push<ToMain>(se, std::forward<decltype(batchEnvs)>(batchEnvs));

			LPVOID newFiber;

			if (Resources::AvailableFibers().size() > 0) {
				newFiber = Resources::AvailableFibers()[Resources::AvailableFibers().size() - 1];
				Resources::AvailableFibers().pop_back();
			}
			else
				newFiber = CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH, (LPFIBER_START_ROUTINE)OpenCallTriggerAndEnterJobLoop, nullptr);

			SwitchToFiber(newFiber);
		}

		inline void FinishCalledJob(LPVOID oldFiber) {
			//Mark for re-use
			Resources::AvailableFibers().push_back(GetCurrentFiber());
			SwitchToFiber(oldFiber);

			//Re-use starts here
			Resources::CallTrigger()->Open();
		}

		//Starting point for a new fiber, queues a list of jobs and immediately enters the job loop.
		//This is used by the Call- functions to delay queueing of jobs until after the calling fiber
		//has been suspended.
		inline void OpenCallTriggerAndEnterJobLoop(LPVOID jobPtr) {
			Resources::CallTrigger()->Open();
			WorkerThread::JobLoop();
		}
	}
}
