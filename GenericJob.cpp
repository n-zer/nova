#include "GenericJob.h"
#include "JobCounter.h"

void Envelope::AddSealedEnvelope(SealedEnvelope se)
{
	m_sealedEnvelopes.push_back(se);
}
