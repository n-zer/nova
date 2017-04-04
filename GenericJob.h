#pragma once

#include <memory>
#include <vector>
#include <tuple>
#include "Job.h"
#include "JobFunction.h"
#include "JobCounterFD.h"
#include "Globals.h"

#pragma region Old Generic Job

struct GenericJob {
	JobFunction m_task;
	JobBase* m_data;
	std::vector<std::shared_ptr<JobCounter>> m_counters;
	GenericJob() {};
	GenericJob(JobFunction task, JobBase* data) : m_task(task), m_data(data) {};

	static void DeleteData(JobBase* jd);

	template <typename ... Ts>
	static void RunJob(JobBase* rawData) {
		TemplatedJobBase<Ts...> * jobData = static_cast<TemplatedJobBase<Ts...>*>(rawData);
		apply(*jobData, jobData->m_tuple);
	}

	template <typename ... Ts>
	static void RunBatchJob(JobBase* rawData) {
		BatchWrapper<Job<unsigned int, unsigned int, Ts...>> * batchWrapper = static_cast<BatchWrapper<Job<unsigned int, unsigned int, Ts...>>*>(rawData);
		unsigned int start = std::get<0>(batchWrapper->job.m_tuple);
		unsigned int end = std::get<1>(batchWrapper->job.m_tuple);
		unsigned int count = end - start;
		unsigned int section = (WorkerThread::GetThreadId() + WorkerThread::GetThreadCount() - batchWrapper->startThread) % WorkerThread::GetThreadCount() + 1;
		unsigned int newStart = static_cast<unsigned int>(start + floorf(count*(section - 1) / batchWrapper->sections));
		end = static_cast<unsigned int>(start + floorf(count*section / batchWrapper->sections));

		std::tuple<Ts...> splitTuple = std::get<Ts...>(batchWrapper->job.m_tuple);
		std::tuple<unsigned int, unsigned int, Ts...> callParams = std::tuple_cat(std::make_tuple(newStart, end), splitTuple);
		apply(batchWrapper->job, callParams);
	}
};

#pragma endregion

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