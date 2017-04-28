#include "Nova.h"
#include <exception>
#include <Windows.h>

namespace Nova {

	void Push(Envelope& e) {
		internal::Resources::m_queue.enqueue(e);
	}

	void Push(Envelope&& e) {
		Push(e);
	}

	void Push(std::vector<Envelope> & envs) {
		internal::Resources::m_queue.enqueue_bulk(envs.begin(), envs.size());
	}

	namespace internal{
		moodycamel::BlockingConcurrentQueue<Envelope> Resources::m_queue;
		thread_local std::vector<LPVOID> Resources::m_availableFibers;
		thread_local std::vector<Envelope> Resources::m_currentJobs;

		void Pop(Envelope &e) {
			internal::Resources::m_queue.wait_dequeue(e);
		}

		void QueueJobsAndEnterJobLoop(LPVOID jobPtr) {
			Push(Resources::m_currentJobs);
			Resources::m_currentJobs.clear();
			internal::WorkerThread::JobLoop();
		}

		void Call(std::vector<Envelope> & envs) {
			if (envs.size() == 0)
				throw std::runtime_error("Attempting to call no Envelopes");

			PVOID currentFiber = GetCurrentFiber();
			auto completionJob = [=]() {
				Push(MakeJob(&FinishCalledJob, currentFiber));
			};

			{
				SealedEnvelope se(Envelope(&Envelope::RunRunnable<decltype(completionJob)>, &completionJob));

				for (Envelope & e : envs)
					e.AddSealedEnvelope(se);

				envs.swap(Resources::m_currentJobs);
			}

			LPVOID newFiber;

			if (Resources::m_availableFibers.size() > 0) {
				newFiber = Resources::m_availableFibers[Resources::m_availableFibers.size() - 1];
				Resources::m_availableFibers.pop_back();
			}
			else
				newFiber = CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH, (LPFIBER_START_ROUTINE)QueueJobsAndEnterJobLoop, nullptr);

			SwitchToFiber(newFiber);
		}

		void FinishCalledJob(LPVOID oldFiber) {
			//Mark for re-use
			Resources::m_availableFibers.push_back(GetCurrentFiber());
			SwitchToFiber(oldFiber);

			//Re-use starts here
			std::vector<Envelope> envs;
			envs.swap(Resources::m_currentJobs);
			Push(envs);
		}
	}
}
