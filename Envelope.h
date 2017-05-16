#pragma once

#include <memory>

namespace Nova {
	class Envelope;
	namespace internal{
		class SealedEnvelope {
		public:
			SealedEnvelope() {}
			SealedEnvelope(Envelope & e);
			void Open();
		private:
			struct Seal;
			std::shared_ptr<Seal> m_seal;
		};
	}

	class Envelope {
	public:
		Envelope() {}
		Envelope(const Envelope &) = delete;
		Envelope operator=(const Envelope &) = delete;
		Envelope(Envelope && e) noexcept {
			//To-do, still need to delete the current object's runnable
			m_runFunc = e.m_runFunc;
			m_runnable = e.m_runnable;
			m_sealedEnvelope = std::move(e.m_sealedEnvelope);
			e.m_runFunc = &Envelope::NoOp;
			e.m_runnable = nullptr;
		}
		Envelope& operator=(Envelope && e) noexcept {
			//To-do, still need to delete the current object's runnable
			m_runFunc = e.m_runFunc;
			m_runnable = e.m_runnable;
			m_sealedEnvelope = std::move(e.m_sealedEnvelope);
			e.m_runFunc = &Envelope::NoOp;
			e.m_runnable = nullptr;
			return *this;
		}

		template<typename Runnable,	std::enable_if_t<!std::is_same<std::decay_t<Runnable>, Envelope>::value, int> = 0>
		Envelope(Runnable&& runnable)
			: m_runFunc(&Envelope::RunAndDeleteRunnable<std::decay_t<Runnable>>), m_runnable(new std::decay_t<Runnable>(std::forward<Runnable>(runnable))){
		}

		template<typename Runnable,	std::enable_if_t<!std::is_same<std::decay_t<Runnable>, Envelope>::value, int> = 0>
		Envelope(Runnable * runnable)
			: m_runFunc(&Envelope::RunRunnable<std::decay_t<Runnable>>), m_runnable(runnable) {
		}

		Envelope(void(*runFunc)(void*), void * runnable)
			: m_runFunc(runFunc), m_runnable(runnable) {
		}

		void operator () () const {
			m_runFunc(m_runnable);
		}

		void AddSealedEnvelope(internal::SealedEnvelope & se);
		void OpenSealedEnvelope();

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

		static void NoOp(void * runnable) {};

	private:
		void(*m_runFunc)(void *);
		void * m_runnable;
		internal::SealedEnvelope m_sealedEnvelope;
	};

	struct internal::SealedEnvelope::Seal {
		Seal(Envelope & e)
			: m_env(std::move(e)) {
		}

		~Seal() {
			m_env();
		}

		Envelope m_env;
	};
}