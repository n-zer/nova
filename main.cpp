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

void Job1(unsigned int number, unsigned int c, float fl) {
	printf(((std::to_string(WorkerThread::GetThreadId())+" - "+std::to_string(1)+"\n").c_str()));
}

void Job2(bool yes) {
	printf(((std::to_string(WorkerThread::GetThreadId())).c_str()));
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
		//push standalone job
		//for (unsigned c = 0; c < 100000; c++)
		//	JobQueuePool::PushJob(MakeJob(&TestTemplatedJobs, 0, 0, 0), se);
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
	
	JobQueuePool::PushJobs(MakeJob(&Job3), MakeJob(&Job3));
	
}


int main() {
	Init(&InitialJob);
}
