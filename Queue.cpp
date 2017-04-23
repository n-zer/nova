#include "Queue.h"
#include <exception>

namespace Nova {
	moodycamel::BlockingConcurrentQueue<Envelope> Queue::m_queue;
	thread_local std::vector<LPVOID> Queue::m_availableFibers;
	thread_local std::vector<Envelope> Queue::m_currentJobs;

	void Queue::PushEnvelopes(Envelope& e) {
		m_queue.enqueue(e);
	}

	void Queue::PushEnvelopes(std::vector<Envelope> & envs)	{
		m_queue.enqueue_bulk(envs.begin(), envs.size());
	}

	void Queue::PopJob(Envelope &e) {
		m_queue.wait_dequeue(e);
	}

	void Queue::QueueJobsAndEnterJobLoop(LPVOID jobPtr) {
		PushEnvelopes(m_currentJobs);
		m_currentJobs.clear();
		internal::WorkerThread::JobLoop();
	}

	void Queue::Call(std::vector<Envelope> & envs) {
		if (envs.size() == 0)
			throw std::runtime_error("Attempting to call no Envelopes");

		PVOID currentFiber = GetCurrentFiber();
		auto completionJob = [=]() {
			Push(MakeJob(&FinishCalledJob, currentFiber));
		};

		{
			SealedEnvelope se(Envelope(&Envelope::RunRunnable<decltype(completionJob)>, &completionJob));

			for (Envelope & e : envs)
				e.AddSealedEnvelope(se);

			envs.swap(m_currentJobs);
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
		std::vector<Envelope> envs;
		envs.swap(m_currentJobs);
		PushEnvelopes(envs);
	}
}
