#include "Nova.h"
#include <exception>
#include <Windows.h>

namespace Nova {

	void Push(Envelope& e) {
		using namespace internal;
		Resources::m_queue.Push(e);
	}

	void Push(Envelope&& e) {
		Push(e);
	}

	void Push(std::vector<Envelope> & envs) {
		using namespace internal;
		Resources::m_queue.Push(envs);
	}

	namespace internal{
		QueueWrapper<Envelope> Resources::m_queue;
		thread_local std::vector<LPVOID> Resources::m_availableFibers;
		thread_local SealedEnvelope * Resources::m_callTrigger;

		void Pop(Envelope &e) {
			using namespace internal;
			Resources::m_queue.Pop(e);
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

		void Push(internal::SealedEnvelope & se, std::vector<Envelope> & envs) {
			for (Envelope & e : envs)
				e.AddSealedEnvelope(se);
			internal::Resources::m_queue.Push(envs);
			for (Envelope & e : envs)
				e.OpenSealedEnvelope();
		}
	}
}
