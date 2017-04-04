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


void TestTemplatedJobs(unsigned int number, unsigned int c, float fl) {
	printf((std::to_string(number + fl + c).c_str()));
	//JobQueuePool::PushJobAsBatch(Job<unsigned int, unsigned int, float>(&TestTemplatedJobs, 0, 1000, 6.0f));
}

struct Test {
	int two = 2;
	void TestFunction(int otherTwo) {
		two = otherTwo;
	}
};


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


	CreateConsoleWindow(500, 120, 32, 120);


#ifdef _DEBUG
	//set number of threads
	unsigned int threadCount = 1;
#else
	unsigned int threadCount = std::thread::hardware_concurrency();
#endif

	//create job queues
	JobQueuePool::InitPool(threadCount);	
	
	//push standalone job
	JobQueuePool::PushJob(&TestTemplatedJobs, unsigned int(0), unsigned int(1000), 6.0f);

	//push member job
	Test test;
	JobQueuePool::PushJob(MakeJob(&Test::TestFunction, test, 4)); //takes object to use as this pointer as second param

	//create threads
	vector<WorkerThread> threads;
	threads.resize(threadCount - 1);

	//main thread works too
	WorkerThread::JobLoop();
}
