#include <Windows.h>
#include <time.h>
#include <thread>
#include <iostream>
#include <vector>
#include <string>
#include "JobQueue.h"


void TestTemplatedJobs(unsigned int number, unsigned int c, float fl) {
	printf(((std::to_string(number + fl + c)+" ").c_str()));
	JobQueuePool::PushJob(&TestTemplatedJobs, 0, 0, 0);
}

struct Test {
	int two = 2;
	void TestFunction(int otherTwo) {
		two = otherTwo;
	}
};

Test test;

void InitialJob() {
	//push standalone job
	for (unsigned c = 0; c < WorkerThread::GetThreadCount(); c++)
		JobQueuePool::PushJob(&TestTemplatedJobs, 0, 0, 0);

	//push member job
	JobQueuePool::PushJob(&Test::TestFunction, &test, 4); //takes object to use as this pointer as second param

	//push batch job
	JobQueuePool::PushJobAsBatch(MakeBatchJob(&TestTemplatedJobs, 0, 1000, 6.0f));
}


int main() {
	Init(&InitialJob);
}
