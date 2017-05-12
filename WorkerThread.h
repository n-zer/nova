#pragma once

#include <thread>
#include <mutex>
#include <Windows.h>

namespace Nova {
	namespace internal {
		class WorkerThread {
		public:
			WorkerThread();
			static void JobLoop();
			static unsigned int GetThreadId();
			static unsigned int GetThreadCount();
			static void KillWorker();
			void Join();
		private:
			static void InitThread();
			static thread_local unsigned int s_thread_id;
			static thread_local bool s_running;
			static unsigned int s_threadCount;
			static std::mutex s_initLock;
			std::thread m_thread;
		};
	}
}
