//#define NOMINMAX

#include <Windows.h>
#include <time.h>
#include <thread>
#include <iostream>
#include <vector>
#include "Game.h"
#include "JobQueue.h"
#include "BatchJobData.h"
#include "JobCounter.h"

vector<unsigned int> printTest;

void PrintStuff(void* data) {
	unsigned int num = 0;
	for (unsigned int c = 0; c < 10000000; c++)
		num++;
	printf(std::to_string(num).c_str());
	JobQueuePool::PushJob({ &PrintStuff , nullptr });
}

void PrintStuffBatch(void* bjd) {
	BatchJobData& bjdr = *static_cast<BatchJobData*>(bjd);
	for (unsigned int c = bjdr.start; c < bjdr.start + bjdr.count; c++) {
		for (unsigned int n = 0; n < 10000000; n++)
			printTest[c]++;
		printf(std::to_string(printTest[c]).c_str());
	}
}


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

	printTest.resize(2);

	//set number of threads
	unsigned int threadCount = std::thread::hardware_concurrency();

	//create job queues
	JobQueuePool::InitPool(threadCount);	

	//push initialization logic to the queues
	/*for (unsigned int c = 0; c < threadCount; c++) {
		JobQueuePool::PushJob({ &PrintStuff });
	}*/
	JobCounter printCounter;
	BatchJobData* printJobData = new BatchJobData{ unsigned int(0), printTest.size() };
	Job batchJob = { PrintStuffBatch, printJobData, &printCounter };
	printCounter.Init(JobQueuePool::PushBatchJob, batchJob, 5);
	JobQueuePool::PushBatchJob(batchJob);

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
