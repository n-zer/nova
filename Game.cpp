#include "Game.h"

void Game::Init()
{
}

void Game::Update(float deltaTime, float totalTime)
{
}

Game::Game(HINSTANCE hInstance)
	: DXCore(
		hInstance,		   // The application's handle
		"DirectX Game",			   // Height of the window's client area
		true)			   // Show extra stats (fps) in title bar?
{
#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.");
#endif
}

void Game::UpdateTitleBarForGame(std::string in)
{
}
