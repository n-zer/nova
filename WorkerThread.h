#pragma once

#include <thread>
#include <mutex>
#include <Windows.h>
#include "Envelope.h"
#include "Nova.h"

namespace Nova {
	namespace internal {
		//Forward declarations
		void PopMain(Envelope & e);
		void Pop(Envelope & e);

		class WorkerThread {
		public:
			WorkerThread() {
				m_thread = std::thread(InitThread);
			}
			static void JobLoop() {
				while (Running()) {
					Envelope e;
					if (WorkerThread::GetThreadId() == 0)
						PopMain(e);
					else
						Pop(e);
					e();
				}
			}
			static std::size_t GetThreadId() {
				return ThreadId();
			}
			static std::size_t GetThreadCount() {
				return ThreadCount();
			}
			static void KillWorker() {
				Running() = false;
			}
			void Join() {
				m_thread.join();
			}
		private:
			static void InitThread() {
				{
					std::lock_guard<std::mutex> lock(InitLock());
					ThreadId() = ThreadCount();
					ThreadCount()++;
				}
				ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
				JobLoop();
			}

			//Meyers singletons
			static std::size_t & ThreadId() {
				static thread_local std::size_t id = 0;
				return id;
			}
			static bool & Running() {
				static thread_local bool running = true;
				return running;
			}
			static std::size_t & ThreadCount() {
				static std::size_t count = 0;
				return count;
			}
			static std::mutex & InitLock() {
				static std::mutex lock;
				return lock;
			}

			std::thread m_thread;
		};
	}
}
