#include <Windows.h>
#include "WorkerThread.h"
#include "Envelope.h"
#include "JobQueue.h"

thread_local unsigned int WorkerThread::s_thread_id = 0;
unsigned int WorkerThread::s_threadCount = 1;
std::mutex WorkerThread::s_countLock;

WorkerThread::WorkerThread() {
	m_thread = std::thread(InitThread);
}

void WorkerThread::JobLoop() {
	
	while (true) {
		Envelope e;
		JobQueuePool::PopJob(e);
		e();
	}
}

unsigned int WorkerThread::GetThreadId() {
	return s_thread_id;
}

unsigned int WorkerThread::GetThreadCount()
{
	return s_threadCount;
}

void WorkerThread::InitThread()
{
	{
		std::lock_guard<std::mutex> lock(s_countLock);
		s_thread_id = s_threadCount;
		s_threadCount++;
	}
	ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
	JobLoop();
}
