//#define NOMINMAX

#include <Windows.h>
#include <time.h>
#include <thread>
#include <iostream>
#include <vector>
#include "Game.h"
#include "JobQueue.h"
#include "Job.h"
#include "JobCounter.h"
#include "WorkerThread.h"

vector<unsigned int> printTest;

void TestTemplatedJobs(unsigned int number, unsigned int c, float fl) {
	printf((std::to_string(number + fl + c).c_str()));
	//JobQueuePool::PushJobAsBatch(Job<unsigned int, unsigned int, float>(&TestTemplatedJobs, 0, 1000, 6.0f));
}

//void PrintStuff(JobDataBase* data) {
//	unsigned int num = 0;
//	for (unsigned int c = 0; c < 10000000; c++)
//		num++;
//	printf(std::to_string(num).c_str());
//	JobQueuePool::PushJob(GenericJob{ &PrintStuff , nullptr });
//}
//
//void PrintStuffBatch(unsigned int start, unsigned int count) {
//	for (unsigned int c = start; c < start + count; c++) {
//		for (unsigned int n = 0; n < 100000000; n++)
//			printTest[c]++;
//		printf((std::to_string(printTest[c]) + "\n").c_str());
//	}
//}
//
//void QueuePrintJob(JobDataBase* data) {
//	PrintData* pd = new PrintData{};
//	BatchJobData pJobData(0, printTest.size(), pd);
//	GenericJob batchJob = { PrintStuffBatch, &pJobData };
//	std::shared_ptr<JobCounter> jc = std::make_shared<JobCounter>(GenericJob{ QueuePrintJob,nullptr });
//	batchJob.m_counters.push_back(jc);
//	JobQueuePool::PushJobAsBatch(batchJob);
//	int test = 0;
//}
//
//void Test(int number, int otherNumber, float decimalNumber) {
//	printf(std::to_string(number + otherNumber + decimalNumber).c_str());
//}


void CreateConsoleWindow(int bufferLines, int bufferColumns, int windowLines, int windowColumns)
{
	// Our temp console info struct
	CONSOLE_SCREEN_BUFFER_INFO coninfo;

	// Get the console info and set the number of lines
	AllocConsole();
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
	coninfo.dwSize.Y = bufferLines;
	coninfo.dwSize.X = bufferColumns;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

	SMALL_RECT rect;
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = windowColumns;
	rect.Bottom = windowLines;
	SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, &rect);

	FILE *stream;
	freopen_s(&stream, "CONIN$", "r", stdin);
	freopen_s(&stream, "CONOUT$", "w", stdout);
	freopen_s(&stream, "CONOUT$", "w", stderr);

	// Prevent accidental console window close
	HWND consoleHandle = GetConsoleWindow();
	HMENU hmenu = GetSystemMenu(consoleHandle, FALSE);
	EnableMenuItem(hmenu, SC_CLOSE, MF_GRAYED);
}


struct Test {
	int two = 2;
	void TestFunction(int otherTwo) {
		two = otherTwo;
	}
};



// --------------------------------------------------------
// Entry point for a graphical (non-console) Windows application
// --------------------------------------------------------
int WINAPI WinMain(
	HINSTANCE hInstance,		// The handle to this app's instance
	HINSTANCE hPrevInstance,	// A handle to the previous instance of the app (always NULL)
	LPSTR lpCmdLine,			// Command line params
	int nCmdShow)				// How the window should be shown (we ignore this)
{
#if defined(DEBUG) | defined(_DEBUG)
	// Enable memory leak detection as a quick and dirty
	// way of determining if we forgot to clean something up
	//  - You may want to use something more advanced, like Visual Leak Detector
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	srand(time(NULL));

	// Create the Game object using
	// the app handle we got from WinMain
	//Game dxGame(hInstance);

	// Result variable for function calls below
	//HRESULT hr = S_OK;
	CreateConsoleWindow(500, 120, 32, 120);
	// Attempt to create the window for our program, and
	// exit early if something failed
	//hr = dxGame.InitWindow();
	//if (FAILED(hr)) return hr;

	// Attempt to initialize DirectX, and exit
	// early if something failed
	//hr = dxGame.InitDirectX();
	//if (FAILED(hr)) return hr;

	//SetCapture(hInstance);

	// Begin the message and game loop, and then return
	// whatever we get back once the game loop is over
	//return dxGame.Run();

	auto tup = std::make_tuple(5, 6, 4.0f);

#ifdef _DEBUG
	//set number of threads
	unsigned int threadCount = 2;
#else
	unsigned int threadCount = std::thread::hardware_concurrency();
#endif


	printTest.resize(threadCount);

	//create job queues
	JobQueuePool::InitPool(threadCount);	
	Test jobTest;
	MemberJob<Test, int> testJob(&Test::TestFunction, &jobTest, 0);

	JobQueuePool::PushJobAsBatch(Job<unsigned int, unsigned int, float>(&TestTemplatedJobs, 0, 1000, 6.0f));
	//QueuePrintJob(nullptr);
	//push initialization logic to the queues
	/*for (unsigned int c = 0; c < threadCount; c++) {
		JobQueuePool::PushJob({ &PrintStuff });
	}*/

	

	//create threads
	vector<WorkerThread> threads;
	threads.resize(threadCount - 1);

	//main thread works too
	WorkerThread::JobLoop();
}

/*static float fRand(float min, float max) {
float f = (float)rand() / RAND_MAX;
return min + f * (max - min);
}*/
