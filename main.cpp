#include <Windows.h>
#include <time.h>
#include <thread>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include "JobQueue.h"
#include "WorkerThread.h"

void Job1(unsigned int number, unsigned int c, float fl) {
	printf(((std::to_string(WorkerThread::GetThreadId())+" - "+std::to_string(1)+"\n").c_str()));
}

void Job2(bool yes) {
	printf(((std::to_string(WorkerThread::GetThreadId())).c_str()));
}

void parallelForTest(unsigned index, unsigned index2, bool test) {
	printf((std::to_string(index) + " - " + std::to_string(index2) + "\n").c_str());
}


void Job3() {
	while (true) {
		JobQueuePool::CallJobs(
			MakeJob(&Job1, 0, 0, 0),
			MakeJob(&Job2, true),
			MakeJob(&Job1, 0, 0, 0),
			MakeJob(&Job2, true),
			MakeJob(&Job1, 0, 0, 0),
			MakeJob(&Job2, true),
			MakeJob(&Job1, 0, 0, 0),
			MakeJob(&Job2, true),
			MakeJob(&Job1, 0, 0, 0),
			MakeJob(&Job2, true),
			MakeJob(&Job1, 0, 0, 0),
			MakeJob(&Job2, true),
			MakeJob(&Job1, 0, 0, 0),
			MakeJob(&Job2, true),
			MakeJob(&Job1, 0, 0, 0),
			MakeJob(&Job2, true)
		);

		//JobQueuePool::ParallelFor(&parallelForTest, 0, 500, true);

		printf("call finished");
	}
}

struct Test {
	int two = 2;
	void TestFunction(int otherTwo) {
		two = otherTwo;
	}
};

Test test;

void InitialJob() {
	
	//JobQueuePool::PushJobs(MakeJob(&Job3), MakeJob(&Job3));
	JobQueuePool::PushJobAsBatch(MakeBatchJob(&parallelForTest, 0, 800, true));
	JobQueuePool::PushJobAsBatch(MakeBatchJob(&parallelForTest, 0, 800, true));
	JobQueuePool::PushJobAsBatch(MakeBatchJob(&parallelForTest, 0, 800, true));
	//while (true)
		//JobQueuePool::CallJobs(MakeBatchJob(&parallelForTest, 0, 500, true));
}


int main() {
	Init(&InitialJob);
}
