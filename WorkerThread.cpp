#include "WorkerThread.h"
#include "Envelope.h"
#include "Nova.h"
#include <Windows.h>

namespace Nova {
	namespace internal {
		thread_local unsigned int WorkerThread::s_thread_id = 0;
		thread_local bool WorkerThread::s_running = true;
		unsigned int WorkerThread::s_threadCount = 1;
		std::mutex WorkerThread::s_initLock;

		WorkerThread::WorkerThread() {
			m_thread = std::thread(InitThread);
		}

		void WorkerThread::JobLoop() {
			while (s_running) {
				Envelope e;
				Pop(e);
				e();
			}
		}

		unsigned int WorkerThread::GetThreadId() {
			return s_thread_id;
		}

		unsigned int WorkerThread::GetThreadCount() {
			return s_threadCount;
		}

		void WorkerThread::InitThread() {
			{
				std::lock_guard<std::mutex> lock(s_initLock);
				s_thread_id = s_threadCount;
				s_threadCount++;
			}
			ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
			JobLoop();
		}

		void WorkerThread::KillWorker() {
			s_running = false;
		}

		void WorkerThread::Join() {
			m_thread.join();
		}
	}
}
