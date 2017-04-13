#include "JobQueue.h"
#include "WorkerThread.h"
#include "CriticalLock.h"

unsigned int JobQueuePool::m_size = 0;
std::vector<JobQueue> JobQueuePool::m_queues;
thread_local std::vector<LPVOID> JobQueuePool::m_availableFibers;
thread_local std::vector<Envelope> * JobQueuePool::m_currentJobs;

JobQueue::JobQueue()
{
	InitializeCriticalSection(&m_lock);
	InitializeConditionVariable(&m_cv);
}

JobQueue::JobQueue(const JobQueue & other)
{
	m_jobs = other.m_jobs;
}

void JobQueue::PopJob(Envelope &e)
{
	CriticalLock clock(m_lock);

	while (m_jobs.size() == 0) {
		SleepConditionVariableCS(&m_cv, &m_lock, INFINITE);
	}

	e = m_jobs[0];
	m_jobs.pop_front();
}

void JobQueue::PushJob(Envelope e)
{
	{
		CriticalLock clock(m_lock);
		m_jobs.push_back(e);
	}
	WakeConditionVariable(&m_cv);
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
	return m_queues[WorkerThread::GetThreadId()].PopJob(e);
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
	m_size = numCores;
	m_queues.resize(numCores);
}
