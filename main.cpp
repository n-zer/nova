#include <Windows.h>
#include <time.h>
#include <thread>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <functional>
#include <ppl.h>
#include "JobQueue.h"

std::chrono::time_point<std::chrono::steady_clock> start;
long long jobTime;

unsigned testNum = 0;

void ParallelForSingleIteration(unsigned index) {
	printf((std::to_string(index)).c_str());
}

void ParallelForTest(unsigned start, unsigned end) {
	for (unsigned c = start; c < end; c++)
		ParallelForSingleIteration(c);
}

void TestTemplatedJobs(unsigned int number, unsigned int c, float fl) {
	printf(((std::to_string(number + fl + c)+"\n").c_str()));
	//JobQueuePool::PushJob(&TestTemplatedJobs, 0, 0, 0);
}

struct Test {
	int two = 2;
	void TestFunction(int otherTwo) {
		two = otherTwo;
	}
};

Test test;

void InitialJob() {
	
	JobQueuePool::PushJobs(
		MakeBatchJob(&TestTemplatedJobs, 0, 1000, 6),
		MakeJob(&TestTemplatedJobs, 0, 0, 0),
		MakeJob(&TestTemplatedJobs, 1, 0, 0),
		MakeJob(&TestTemplatedJobs, 2, 0, 0),
		MakeJob(&TestTemplatedJobs, 3, 0, 0),
		MakeJob(&TestTemplatedJobs, 4, 0, 0),
		MakeJob(&TestTemplatedJobs, 5, 0, 0),
		MakeJob(&TestTemplatedJobs, 6, 0, 0),
		MakeJob(&TestTemplatedJobs, 7, 0, 0)
	);
	//push standalone job
	//for (unsigned c = 0; c < 100000; c++)
	//	JobQueuePool::PushJob(MakeJob(&TestTemplatedJobs, 0, 0, 0), se);

	
}


int main() {
	Init(&InitialJob);
}
