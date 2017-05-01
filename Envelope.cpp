#include "Envelope.h"
namespace Nova {
	void Envelope::AddSealedEnvelope(internal::SealedEnvelope & se) {
		m_sealedEnvelope = se;
	}

	void Envelope::OpenSealedEnvelope() {
		m_sealedEnvelope.Open();
	}

	internal::SealedEnvelope::SealedEnvelope(Envelope e)
		: m_seal(std::make_shared<Seal>(e)) {
	}

	void internal::SealedEnvelope::Open() {
		m_seal.reset();
	}
}
