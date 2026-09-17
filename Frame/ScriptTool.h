#pragma once
#include <string>

// C++ スクリプト（コンポーネント）の作成と、Visual Studio で開く処理
class ScriptTool
{
public:
	// folder に ClassName.h / ClassName.cpp をひな形から作り、vcxproj に追加する
	static bool CreateScript(const std::string& folder, const std::string& className, std::string& outError);

	// Visual Studio（見つからなければ関連付けられたアプリ）でファイルを開く
	static void OpenInEditor(const std::string& path);

	// コンポーネントの型名から .cpp を探す（見つからなければ空文字）
	static std::string FindSourceFile(const std::string& typeName);

	// C++ のクラス名として使えるか
	static bool IsValidClassName(const std::string& name);
};
