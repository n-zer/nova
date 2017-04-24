#include "Envelope.h"
namespace Nova {
	void Envelope::AddSealedEnvelope(internal::SealedEnvelope & se) {
		m_sealedEnvelope = se;
	}

	internal::SealedEnvelope::SealedEnvelope(Envelope e)
		: m_seal(std::make_shared<Seal>(e)) {
	}
}
