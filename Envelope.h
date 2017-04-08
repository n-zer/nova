#pragma once

#include <memory>
#include <vector>

class SealedEnvelope;

class Envelope {
public:
	Envelope(){}

	template<typename Runnable>
	Envelope(Runnable * runnable)
		: m_runFunc(&Envelope::RunRunnable<Runnable>), m_runnable(runnable) {

	}

	Envelope(void(*runFunc)(void*), void * runnable)
		: m_runFunc(runFunc), m_runnable(runnable) {

	}

	void operator () () {
		m_runFunc(m_runnable);
	}

	void AddSealedEnvelope(SealedEnvelope se);

	template <typename Runnable>
	static void RunRunnable(void * runnable) {
		(static_cast<Runnable*>(runnable))->operator()();
	}

	template <typename Runnable>
	static void DeleteRunnable(void * runnable) {
		delete static_cast<Runnable*>(runnable);
	}

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