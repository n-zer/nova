#include "Queue.h"
#include "CriticalLock.h"

namespace Nova {
	moodycamel::BlockingConcurrentQueue<Envelope> Queue::m_queue;
	thread_local std::vector<LPVOID> Queue::m_availableFibers;
	thread_local std::vector<Envelope> Queue::m_currentJobs;

	//Take a job and push it to neighboring queue
	void Queue::PushEnvelopes(Envelope& e) {
		m_queue.enqueue(e);
	}

	void Queue::PushEnvelopes(std::vector<Envelope>& envs)
	{
		m_queue.enqueue_bulk(envs.begin(), envs.size());
	}

	void Queue::PopJob(Envelope &e) {
		m_queue.wait_dequeue(e);
	}

	void Queue::QueueJobsAndEnterJobLoop(LPVOID jobPtr)
	{
		PushEnvelopes(m_currentJobs);
		m_currentJobs.clear();
		WorkerThread::JobLoop();
	}

	void Queue::Call(std::vector<Envelope> & e)
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

	void Queue::FinishCalledJob(LPVOID oldFiber) {
		//Mark for re-use
		m_availableFibers.push_back(GetCurrentFiber());
		SwitchToFiber(oldFiber);

		//Re-use starts here
		PushEnvelopes(m_currentJobs);
		m_currentJobs.clear();
	}
}
