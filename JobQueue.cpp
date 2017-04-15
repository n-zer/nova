#include "JobQueue.h"
#include "WorkerThread.h"
#include "CriticalLock.h"

unsigned int JobQueuePool::m_size = 0;
std::vector<JobQueue> JobQueuePool::m_queues;
thread_local std::vector<LPVOID> JobQueuePool::m_availableFibers;
thread_local std::vector<Envelope> * JobQueuePool::m_currentJobs;
CONDITION_VARIABLE JobQueue::s_cv;

JobQueue::JobQueue()
{
	InitializeCriticalSection(&m_lock);
}

JobQueue::JobQueue(const JobQueue & other)
{
	m_jobs = other.m_jobs;
}

bool JobQueue::PopJob(Envelope &e)
{
	CriticalLock clock(m_lock);

	if (m_jobs.size() > 0) {
		e = m_jobs[0];
		m_jobs.pop_front();
		return true;
	}
	return false;
}

bool JobQueue::SleepAndPopJob(Envelope & j)
{
	CriticalLock clock(m_lock);

	SleepConditionVariableCS(&s_cv, &m_lock, INFINITE);

	return PopJob(j);
}

void JobQueue::PushJob(Envelope e)
{
	{
		CriticalLock clock(m_lock);
		m_jobs.push_back(e);
	}
	WakeConditionVariable(&s_cv);
}

void JobQueuePool::PushJob(Envelope&& e) {
	PushJob(e);
}

//Take a job and push it to neighboring queue
void JobQueuePool::PushJob(Envelope& e) {
	m_queues[WorkerThread::GetThreadId() % m_size].PushJob(e);
}

void JobQueuePool::PushJobs(std::vector<Envelope>& envs)
{
	unsigned id = WorkerThread::GetThreadId();
	for (unsigned c = 0; c < envs.size(); c++)
		m_queues[(id + c + 1) % m_size].PushJob(envs[c]);
}

void JobQueuePool::PopJob(Envelope &e) {
	if (m_queues[WorkerThread::GetThreadId() % m_size].PopJob(e))
		return;
	while (true) {
		for (unsigned c = 1; c < m_size; c++)
			if (m_queues[(WorkerThread::GetThreadId() + c) % m_size].PopJob(e))
				return;
		if (m_queues[WorkerThread::GetThreadId() % m_size].SleepAndPopJob(e))
			return;
	}
}

void JobQueuePool::QueueJobsAndEnterJobLoop(LPVOID jobPtr)
{
	PushJobs(*m_currentJobs);
	m_currentJobs->clear();
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

		m_currentJobs = &e;
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
	PushJobs(*m_currentJobs);
	m_currentJobs->clear();
}

void JobQueuePool::InitPool(unsigned int numCores) {
	JobQueue::Init();
	m_size = numCores;
	m_queues.resize(numCores);
}
