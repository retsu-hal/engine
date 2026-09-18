#pragma once
#include "EngineAPI.h"
#include <string>
#include <vector>

// ゲーム全体の設定（asset/project.json）。エディタで作り、ゲーム用の exe が起動時に読む
class ENGINE_API ProjectSettings
{
public:
	static std::string Title;		// ウィンドウのタイトル
	static std::string StartScene;	// 最初に読み込むシーン
	static std::vector<std::string> Tags;	// Inspector の Tag で選べる名前

	static void AddTag(const std::string& tag);

	static void Load();
	static void Save();
};
