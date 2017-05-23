#include "Nova.h"
#include <exception>
#include <Windows.h>

namespace Nova {
	void Push(internal::Envelope&& e) {
		internal::Resources::m_queue.Push(std::forward<internal::Envelope>(e));
	}

	void Push(std::vector<internal::Envelope> && envs) {
		internal::Resources::m_queue.Push(std::forward<decltype(envs)>(envs));
	}

	namespace internal{
		QueueWrapper<Envelope> Resources::m_queue;
		thread_local std::vector<LPVOID> Resources::m_availableFibers;
		thread_local SealedEnvelope * Resources::m_callTrigger;

		void Pop(Envelope &e) {
			Resources::m_queue.Pop(e);
		}

		void OpenCallTriggerAndEnterJobLoop(LPVOID jobPtr) {
			Resources::m_callTrigger->Open();
			WorkerThread::JobLoop();
		}

		void FinishCalledJob(LPVOID oldFiber) {
			//Mark for re-use
			Resources::m_availableFibers.push_back(GetCurrentFiber());
			SwitchToFiber(oldFiber);

			//Re-use starts here
			Resources::m_callTrigger->Open();
		}

		void Push(SealedEnvelope & se, std::vector<Envelope> && envs) {
			for (Envelope & e : envs)
				e.AddSealedEnvelope(se);
			Resources::m_queue.Push(std::forward<decltype(envs)>(envs));
			for (Envelope & e : envs)
				e.OpenSealedEnvelope();
		}
	}
}
