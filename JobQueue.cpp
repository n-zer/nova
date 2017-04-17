#include "JobQueue.h"
#include "WorkerThread.h"
#include "CriticalLock.h"

moodycamel::BlockingConcurrentQueue<Envelope> JobQueuePool::m_queue;
thread_local std::vector<LPVOID> JobQueuePool::m_availableFibers;
thread_local std::vector<Envelope> JobQueuePool::m_currentJobs;

//Take a job and push it to neighboring queue
void JobQueuePool::PushJob(Envelope& e) {
	m_queue.enqueue(e);
}

void JobQueuePool::PushJobs(std::vector<Envelope>& envs)
{
	m_queue.enqueue_bulk(envs.begin(), envs.size());
}

void JobQueuePool::PopJob(Envelope &e) {
	m_queue.wait_dequeue(e);
}

void JobQueuePool::QueueJobsAndEnterJobLoop(LPVOID jobPtr)
{
	PushJobs(m_currentJobs);
	m_currentJobs.clear();
	WorkerThread::JobLoop();
}

void JobQueuePool::CallJobs(std::vector<Envelope> & e)
{
	PVOID currentFiber = GetCurrentFiber();
	auto completionJob = MakeJob(&FinishCalledJob, GetCurrentFiber());

	{
		SealedEnvelope se(Envelope(&Envelope::RunRunnable<decltype(completionJob)>, &completionJob));

		for (unsigned c = 0; c < e.size(); c++)
			e[c].AddSealedEnvelope(se);

		e.swap(m_currentJobs);
	}

	LPVOID newFiber;
	
	if (m_availableFibers.size() > 0) {
		newFiber = m_availableFibers[m_availableFibers.size() - 1];
		m_availableFibers.pop_back();
	}
	else
		newFiber = CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH, (LPFIBER_START_ROUTINE)QueueJobsAndEnterJobLoop, nullptr);

	SwitchToFiber(newFiber);
}

void JobQueuePool::FinishCalledJob(LPVOID oldFiber) {
	//Mark for re-use
	m_availableFibers.push_back(GetCurrentFiber());
	SwitchToFiber(oldFiber);

	//Re-use starts here
	PushJobs(m_currentJobs);
	m_currentJobs.clear();
}
