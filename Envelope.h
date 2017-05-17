#pragma once

#include <memory>

namespace Nova {
	class Envelope;
	namespace internal{
		class SealedEnvelope {
		public:
			SealedEnvelope() {}
			SealedEnvelope(Envelope & e);
			SealedEnvelope(Envelope && e);
			void Open();
		private:
			struct Seal;
			std::shared_ptr<Seal> m_seal;
		};
	}

	class Envelope {
	public:
		Envelope() {}
		~Envelope() {
			m_deleteFunc(m_runnable);
		}
		Envelope(const Envelope &) = delete;
		Envelope operator=(const Envelope &) = delete;
		Envelope(Envelope && e) noexcept {
			Move(std::forward<Envelope>(e));
		}
		Envelope& operator=(Envelope && e) noexcept {
			m_deleteFunc(m_runnable);
			Move(std::forward<Envelope>(e));
			return *this;
		}

		template<typename Runnable,	std::enable_if_t<!std::is_same<std::decay_t<Runnable>, Envelope>::value, int> = 0>
		Envelope(Runnable&& runnable)
			: m_runFunc(&Envelope::RunRunnable<std::decay_t<Runnable>>), m_deleteFunc(&Envelope::DeleteRunnable<std::decay_t<Runnable>>), m_runnable(new std::decay_t<Runnable>(std::forward<Runnable>(runnable))){
		}

		template<typename Runnable,	std::enable_if_t<!std::is_same<std::decay_t<Runnable>, Envelope>::value, int> = 0>
		Envelope(Runnable * runnable)
			: m_runFunc(&Envelope::RunRunnable<std::decay_t<Runnable>>), m_runnable(runnable) {
		}

		Envelope(void(*runFunc)(void*), void(*deleteFunc)(void*), void * runnable)
			: m_runFunc(runFunc), m_deleteFunc(deleteFunc), m_runnable(runnable) {
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

		static void NoOp(void * runnable) {};

	private:
		void Move(Envelope && e) noexcept {
			m_runFunc = e.m_runFunc;
			m_deleteFunc = e.m_deleteFunc;
			m_runnable = e.m_runnable;
			m_sealedEnvelope = std::move(e.m_sealedEnvelope);
			e.m_runFunc = &Envelope::NoOp;
			e.m_deleteFunc = &Envelope::NoOp;
			e.m_runnable = nullptr;
		}

		void(*m_runFunc)(void *) = &Envelope::NoOp;
		void(*m_deleteFunc)(void*) = &Envelope::NoOp;
		void * m_runnable = nullptr;
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