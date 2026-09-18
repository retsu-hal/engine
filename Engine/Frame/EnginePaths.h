#pragma once
#include "EngineAPI.h"
#include <string>

//=============================================================
// エンジンとプロジェクトの場所
//   開発中の並び:  GM31Engine.sln / bin\Debug\Editor.exe / Engine\shader / Sample（プロジェクト）
//   配布したゲーム: Build\ゲーム.exe / Build\shader / Build\asset
//=============================================================
class ENGINE_API EnginePaths
{
public:
	static std::string GetExeDir();			// Editor.exe（Player.exe）のあるフォルダ
	static std::string GetSolutionDir();	// GM31Engine.sln のあるフォルダ（見つからなければ空）
	static std::string GetEngineDir();		// Engine フォルダ（なければ exe のフォルダ）
	static std::string GetProjectDir();		// 今開いているプロジェクト（＝作業フォルダ）

	// shader\xxx.cso などエンジンに付いてくるファイルを探す
	// 作業フォルダ → exe のフォルダ → Engine フォルダ の順に見て、最初に見つかったパスを返す
	static std::string ResolveEngineFile(const std::string& relativePath);

	static bool Exists(const std::string& path);
};
