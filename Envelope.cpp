#include "Envelope.h"
namespace Nova {

	void Envelope::AddSealedEnvelope(SealedEnvelope se)
	{
		m_sealedEnvelopes.push_back(se);
	}

	void Envelope::AddSealedEnvelopes(std::vector<SealedEnvelope> ses) {
		m_sealedEnvelopes.insert(m_sealedEnvelopes.end(), ses.begin(), ses.end());
	}
}
