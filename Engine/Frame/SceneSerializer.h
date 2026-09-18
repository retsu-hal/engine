#pragma once
#include "EngineAPI.h"
#include <string>
#include "Scene.h"

//=============================================================
// シーン（今あるすべての GameObject）を JSON に保存・読み込みする
//=============================================================
class ENGINE_API SceneSerializer
{
public:
	// 保存できなかったオブジェクト（登録されていない継承クラス）の数を返す
	static int SaveToText(std::string& outText);
	static int SaveToFile(const std::string& path);

	// 今のオブジェクトに追加する形で読み込む（空のシーンの Init から呼ぶ）
	static bool LoadFromText(const std::string& text);
	static bool LoadFromFile(const std::string& path);

	// オブジェクトを子ごと複製する（複製した一番上のオブジェクトを返す）
	static class GameObject* Duplicate(class GameObject* source);
};

// JSON から中身を作るシーン
class ENGINE_API FileScene : public Scene
{
private:
	std::string m_Source;	// ファイルのパス、または JSON の文字列
	bool        m_IsText;

public:
	FileScene(const std::string& source, bool isText) : m_Source(source), m_IsText(isText) {}

	void Init() override
	{
		if (m_IsText) SceneSerializer::LoadFromText(m_Source);
		else          SceneSerializer::LoadFromFile(m_Source);
	}
};
