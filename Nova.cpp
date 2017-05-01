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
		thread_local SealedEnvelope * Resources::m_callTrigger;

		void Pop(Envelope &e) {
			internal::Resources::m_queue.wait_dequeue(e);
		}

		void OpenCallTriggerAndEnterJobLoop(LPVOID jobPtr) {
			Resources::m_callTrigger->Open();
			internal::WorkerThread::JobLoop();
		}

		void FinishCalledJob(LPVOID oldFiber) {
			//Mark for re-use
			Resources::m_availableFibers.push_back(GetCurrentFiber());
			SwitchToFiber(oldFiber);

			//Re-use starts here
			Resources::m_callTrigger->Open();
		}
	}
}
