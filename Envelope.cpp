#include "Envelope.h"
namespace Nova {
	void Envelope::AddSealedEnvelope(SealedEnvelope & se) {
		m_sealedEnvelope = se;
	}

	SealedEnvelope::SealedEnvelope(Envelope e)
		: m_seal(std::make_shared<Seal>(e)) {
	}
}
