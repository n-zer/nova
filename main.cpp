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

void Job4(unsigned number, unsigned c, float fl);

void Job3(unsigned int number, unsigned int c, float fl) {
	printf(((std::to_string(WorkerThread::GetThreadId()) + " - " + std::to_string(3) + "\n").c_str()));
	JobQueuePool::PushJobAsBatch(MakeBatchJob(&Job4, 0, 1000, 0));
}

void Job2(unsigned int number, unsigned int c, float fl) {
	printf(((std::to_string(WorkerThread::GetThreadId()) + " - " + std::to_string(2) + "\n").c_str()));
	JobQueuePool::PushJob(&Job3, 0, 0, 0);
}

void Job1(unsigned int number, unsigned int c, float fl) {
	printf(((std::to_string(WorkerThread::GetThreadId())+" - "+std::to_string(1)+"\n").c_str()));
	JobQueuePool::PushJobs(MakeJob(&Job2, 0, 0, 0), MakeJob(&Job3,0,0,0));
}

float floatRand() {
	static thread_local std::mt19937 generator;
	std::uniform_real_distribution<float> distribution(0, 1);
	return distribution(generator);
}

void Job4(unsigned number, unsigned c, float fl) {
	printf(((std::to_string(WorkerThread::GetThreadId()) + " - " + std::to_string(4) + "\n").c_str()));

	float roll = floatRand();
	if (roll > .66f)
		JobQueuePool::PushJob(&Job1, 0, 0, 0);
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
		MakeJob(&Job1, 0, 0, 0),
		MakeJob(&Job1, 1, 0, 0),
		MakeJob(&Job1, 2, 0, 0),
		MakeJob(&Job1, 3, 0, 0),
		MakeJob(&Job1, 4, 0, 0),
		MakeJob(&Job1, 5, 0, 0),
		MakeJob(&Job1, 6, 0, 0),
		MakeJob(&Job1, 7, 0, 0)
	);
	//push standalone job
	//for (unsigned c = 0; c < 100000; c++)
	//	JobQueuePool::PushJob(MakeJob(&TestTemplatedJobs, 0, 0, 0), se);

	
}


int main() {
	Init(&InitialJob);
}
