#pragma once

#include <memory>
#include <vector>

class SealedEnvelope;

class Envelope {
public:
	Envelope(){}

	Envelope(void(*runFunc)(void*), void * runnable)
		: m_runFunc(runFunc), m_runnable(runnable) {

	}

	void operator () () {
		m_runFunc(m_runnable);
	}

	void AddSealedEnvelope(SealedEnvelope se);

private:
	void(*m_runFunc)(void *);
	void * m_runnable;
	std::vector<SealedEnvelope> m_sealedEnvelopes;
};

struct Seal {
	Seal(Envelope e)
		: m_env(e) {

	}
	~Seal() {
		m_env();
	}
	Envelope m_env;
};

class SealedEnvelope {
public:
	SealedEnvelope(Envelope e)
		: m_seal(std::make_shared<Seal>(e)) {
	}
private:
	std::shared_ptr<Seal> m_seal;
};