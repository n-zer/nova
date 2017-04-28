#pragma once

#include <vector>
#include <Windows.h>

#include "blockingconcurrentqueue.h"
#include "Envelope.h"

namespace Nova {
	namespace internal{
		class Resources {
		public:
			static moodycamel::BlockingConcurrentQueue<Envelope> m_queue;
			static thread_local std::vector<LPVOID> m_availableFibers;
			static thread_local std::vector<Envelope> m_currentJobs;
		};
	}
}