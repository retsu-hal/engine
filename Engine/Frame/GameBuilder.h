#pragma once
#include "EngineAPI.h"
#include <string>

//=============================================================
// ゲーム用の exe を作る（Unity の Build and Run）
//   1. MSBuild で Game 構成（エディタなし）をビルド
//   2. Build フォルダに exe・asset・shader・DLL をまとめる
//   3. 必要なら起動する
// ビルドは別プロセスで行い、毎フレーム様子を見る（エディタは止まらない）
//=============================================================
class ENGINE_API GameBuilder
{
public:
	enum class State { Idle, Building, Succeeded, Failed };

	static void Start(bool runAfterBuild);
	static void Update();			// 毎フレーム呼ぶ
	static State GetState();
	static float GetElapsedSeconds();
	static const std::string& GetOutputFolder();
	static void OpenOutputFolder();
};
