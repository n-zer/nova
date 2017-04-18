#include <Windows.h>
#include <time.h>
#include <thread>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include "Queue.h"

void Job1(unsigned int number, unsigned int c, float fl) {
	printf(((std::to_string(number)+" - "+std::to_string(c)+"\n").c_str()));
}

void Job2(bool yes) {
	printf(((std::to_string(0)).c_str()));
}

void parallelForTest(unsigned index, bool test) {
	printf((std::to_string(index) + " - " + "\n").c_str());
}


void Job3() {
	Nova::SealedEnvelope se(&Job3);
	Nova::Queue::Push(Nova::MakeBatchJob(&Job1, 0, 500, 5.0f), se);
}

struct Test {
	int two = 2;
	void TestFunction(int otherTwo) {
		two = otherTwo;
	}
};

Test test;

void InitialJob() {
	Nova::Queue::Push(&Job3);

	Nova::Queue::PushBatch(&Job1, 0, 500, 5.0f);

	Nova::Queue::Call(
		Nova::MakeBatchJob(&Job1, 0, 500, 0.0f)
	);

	Nova::Queue::ParallelFor(&parallelForTest, 0, 500, true);
}


int main() {
	Nova::Queue::Init(&InitialJob);
}
