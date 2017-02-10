#include "JobCounter.h"

JobCounter::JobCounter()
{
	m_function = nullptr;
	m_counter = nullptr;
}

JobCounter::JobCounter(PushFunction pf, Job j)
{
	m_counter = new std::atomic<unsigned int>;

	m_function = pf;
	m_job = j;

	(*m_counter)++;
}

JobCounter::JobCounter(const JobCounter& other) {
	m_counter = other.m_counter;
	m_function = other.m_function;
	m_job = other.m_job;

	(*m_counter)++;
}

JobCounter & JobCounter::operator=(const JobCounter & other) {
	m_counter = other.m_counter;
	m_function = other.m_function;
	m_job = other.m_job;

	(*m_counter)++;

	return *this;
}

JobCounter::JobCounter(const JobCounter&& other) {
	m_counter = other.m_counter;
	m_function = other.m_function;
	m_job = other.m_job;

	(*m_counter)++;
}

JobCounter & JobCounter::operator=(const JobCounter&& other) {
	m_counter = other.m_counter;
	m_function = other.m_function;
	m_job = other.m_job;

	(*m_counter)++;

	return *this;
}

JobCounter::~JobCounter() {
	if (m_counter != nullptr && (*m_counter)-- == 0) {
		delete m_counter;

		m_function(m_job);
	}

	m_counter = nullptr;
}
