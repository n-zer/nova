#pragma once

#include <memory>
#include <vector>
#include "Job.h"

namespace Nova {
	class SealedEnvelope;

	class Envelope {
	public:
		Envelope() {}

		template<typename Runnable>
		Envelope(Runnable * runnable)
			: m_runFunc(&Envelope::RunRunnable<Runnable>), m_runnable(runnable) {

		}

		Envelope(void(*runFunc)(void*), void * runnable)
			: m_runFunc(runFunc), m_runnable(runnable) {

		}

		void operator () () {
			m_runFunc(m_runnable);
		}

		void AddSealedEnvelopes(SealedEnvelope se);
		void AddSealedEnvelopes(std::vector<SealedEnvelope> ses);

		template <typename Runnable>
		static void RunRunnable(void * runnable) {
			(static_cast<Runnable*>(runnable))->operator()();
		}

		template <typename Runnable>
		static void DeleteRunnable(void * runnable) {
			delete static_cast<Runnable*>(runnable);
		}

		template <typename Runnable>
		static void RunAndDeleteRunnable(void * runnable) {
			Runnable* rnb = static_cast<Runnable*>(runnable);
			rnb->operator()();
			delete rnb;
		}

	private:
		void(*m_runFunc)(void *);
		void * m_runnable;
		std::vector<SealedEnvelope> m_sealedEnvelopes;
	};

	class SealedEnvelope {
	public:
		SealedEnvelope(Envelope e)
			: m_seal(std::make_shared<Seal>(e)) {
		}
		template<typename Callable, typename ... Params>
		SealedEnvelope(Callable callable, Params ... args)
			: m_seal(std::make_shared<Seal>(Envelope(&Envelope::RunAndDeleteRunnable<SimpleJob<Callable, Params...>>, new SimpleJob<Callable, Params...>(callable, args...)))) {

		}
	private:
		struct Seal {
			Seal(Envelope e)
				: m_env(e) {

			}
			~Seal() {
				m_env();
			}
			Envelope m_env;
		};
		std::shared_ptr<Seal> m_seal;
	};
}