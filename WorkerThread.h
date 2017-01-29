#pragma once

#include <thread>
#include <Windows.h>
#include <mutex>
#include "Job.h"
#include "JobQueue.h"

class WorkerThread {
public:
	WorkerThread();
	static void JobLoop();
	static unsigned int GetThreadId();
private:
	static void InitThread();
	static thread_local unsigned int s_thread_id;
	static unsigned int s_threadCount;
	static std::mutex s_countLock;
	std::thread m_thread;
};
