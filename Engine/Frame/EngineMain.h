#pragma once
#include "EngineAPI.h"
#include <windows.h>

//=============================================================
// エンジンの起動から終了まで（ウィンドウ作成・ゲームループ）
// Editor.exe / Player.exe の WinMain はこれを呼ぶだけ
//=============================================================
class ENGINE_API EngineMain
{
public:
	static int Run(HINSTANCE instance, int showCommand);

	// GameScripts.dll（ゲームのスクリプト）が読み込めているか
	static bool IsGameScriptsLoaded();
};
