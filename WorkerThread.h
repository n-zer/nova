#pragma once

#include <thread>
#include <mutex>
#include <Windows.h>

namespace Nova {

	class WorkerThread {
	public:
		WorkerThread();
		static void JobLoop();
		static unsigned int GetThreadId();
		static unsigned int GetThreadCount();
	private:
		static void InitThread();
		static thread_local unsigned int s_thread_id;
		static unsigned int s_threadCount;
		static std::mutex s_countLock;
		std::thread m_thread;
	};
}
