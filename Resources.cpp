#include "Resources.h"

namespace Nova {
	namespace internal{
		moodycamel::BlockingConcurrentQueue<Envelope> Resources::m_queue;
		thread_local std::vector<LPVOID> Resources::m_availableFibers;
		thread_local std::vector<Envelope> Resources::m_currentJobs;
	}
}