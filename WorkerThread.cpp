#include "WorkerThread.h"

unsigned int WorkerThread::s_thread_id = 0;
unsigned int WorkerThread::s_threadCount = 1;

WorkerThread::WorkerThread() {
	{
		lock_guard<mutex> lock(s_countLock);
		s_thread_id = s_threadCount;
		s_threadCount++;
	}
	m_thread = std::thread(JobLoop);
}

void WorkerThread::JobLoop() {
	Job j;
	while (true) {
		if (JobQueuePool::PopJob(j)) {
			j.m_task(j.m_data);
			delete j.m_data;
			j.m_data = nullptr;
		}
	}
}

unsigned int WorkerThread::GetThreadId() {
	return s_thread_id;
}