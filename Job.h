#pragma once

#include <memory>
#include <vector>
#include <tuple>
#include "JobData.h"
#include "JobFunction.h"
#include "JobCounterFD.h"
#include "Globals.h"

struct GenericJob {
	JobFunction m_task;
	JobBase* m_data;
	std::vector<std::shared_ptr<JobCounter>> m_counters;
	GenericJob() {};
	GenericJob(JobFunction task, JobBase* data) : m_task(task), m_data(data) {};

	static void DeleteData(JobBase* jd);

	template <typename ... Ts>
	static void RunJob(JobBase* rawData) {
		Job<Ts...> * jobData = static_cast<Job<Ts...>*>(rawData);
		apply(jobData->m_func, jobData->m_tuple);
	}

	template <unsigned int Section, unsigned int Sections, typename ... Ts>
	static void RunBatchJob(JobBase* rawData) {
		Job<unsigned int, unsigned int, Ts...> * jobData = static_cast<Job<unsigned int, unsigned int, Ts...>*>(rawData);
		unsigned int start = std::get<0>(jobData->m_tuple);
		unsigned int end = std::get<1>(jobData->m_tuple);
		unsigned int count = end - start;
		start = static_cast<unsigned int>(start + floorf(count*(Section - 1) / Sections));
		end = static_cast<unsigned int>(start + floorf(count*Section / Sections));

		std::tuple<Ts...> splitTuple = std::get<Ts...>(jobData->m_tuple);
		std::tuple<unsigned int, unsigned int, Ts...> callParams = std::tuple_cat(std::make_tuple(start, end), splitTuple);
		apply(jobData->m_func, callParams);
	}
};

