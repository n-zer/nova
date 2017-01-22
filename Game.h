#include "DXCore.h"

class  Game : public DXCore {
public:
	void Init();
	void Update(float deltaTime, float totalTime);
	Game(HINSTANCE hInstance);
private:
	void UpdateTitleBarForGame(std::string in);
};