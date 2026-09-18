//=============================================================
// Editor.exe（Game 構成では Player.exe）の入口
// 中身はすべて Engine.dll にあるので、ここは呼ぶだけ
//=============================================================
#include <windows.h>
#include "EngineMain.h"

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	return EngineMain::Run(hInstance, nCmdShow);
}
