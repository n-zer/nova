#include "Nova.h"
#include <exception>
#include <Windows.h>

namespace Nova {
	namespace internal{
		QueueWrapper<Envelope> Resources::m_queueWrapper;
		thread_local std::vector<LPVOID> Resources::m_availableFibers;
		thread_local SealedEnvelope * Resources::m_callTrigger;

		void Pop(Envelope &e) {
			Resources::m_queueWrapper.Pop(e);
		}

		void PopMain(Envelope &e) {
			Resources::m_queueWrapper.PopMain(e);
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
	}
}
