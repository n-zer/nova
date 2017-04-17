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

void parallelForTest(unsigned index, bool test) {
	printf((std::to_string(index) + " - " + "\n").c_str());
}


void Job3() {
	SealedEnvelope se(&Job3);
	JobQueuePool::PushJobAsBatch(MakeBatchJob(&Job1, 0, 500, 5), se);
}

struct Test {
	int two = 2;
	void TestFunction(int otherTwo) {
		two = otherTwo;
	}
};

Test test;

void InitialJob() {
	JobQueuePool::PushJob(&Job3);
	while (true) {
		/*JobQueuePool::CallJobs(
			MakeBatchJob(&Job1, 0, 500, 0)
		);

		JobQueuePool::ParallelFor(&parallelForTest, 0, 500, true);*/

		//printf("call finished");
	}
}


int main() {
	Init(&InitialJob);
}
