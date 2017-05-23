#include "Envelope.h"
namespace Nova {
	namespace internal{
		void Envelope::AddSealedEnvelope(SealedEnvelope & se) {
			m_sealedEnvelope = se;
		}

		void Envelope::OpenSealedEnvelope() {
			m_sealedEnvelope.Open();
		}
	}

	SealedEnvelope::SealedEnvelope(internal::Envelope & e)
		: m_seal(std::make_shared<Seal>(e)) {
	}

	SealedEnvelope::SealedEnvelope(internal::Envelope && e)
		: m_seal(std::make_shared<Seal>(std::forward<internal::Envelope>(e))) {
	}

	void SealedEnvelope::Open() {
		m_seal.reset();
	}
}
