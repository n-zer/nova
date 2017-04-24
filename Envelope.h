#pragma once

#include <memory>
#include <vector>

namespace Nova {
	class Envelope;
	class SealedEnvelope {
	public:
		SealedEnvelope() {}
		SealedEnvelope(Envelope e);
	private:
		struct Seal;
		std::shared_ptr<Seal> m_seal;
	};

	class Envelope {
	public:
		Envelope() {}

		template<typename Runnable>
		Envelope(Runnable runnable)
			: m_runFunc(&Envelope::RunAndDeleteRunnable<Runnable>), m_runnable(new Runnable(runnable)) {
		}

		template<typename Runnable>
		Envelope(Runnable * runnable)
			: m_runFunc(&Envelope::RunRunnable<Runnable>), m_runnable(runnable) {
		}

		Envelope(void(*runFunc)(void*), void * runnable)
			: m_runFunc(runFunc), m_runnable(runnable) {
		}

		void operator () () const {
			m_runFunc(m_runnable);
		}

		void AddSealedEnvelope(SealedEnvelope & se);

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
		SealedEnvelope m_sealedEnvelope;
	};

	struct SealedEnvelope::Seal {
		Seal(Envelope e)
			: m_env(e) {
		}

		~Seal() {
			m_env();
		}

		Envelope m_env;
	};
}