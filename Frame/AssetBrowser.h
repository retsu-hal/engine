#pragma once
#include <string>
#include <vector>
#include "Vector3.h"

class GameObject;

// asset フォルダの中身を表示し、ドラッグ＆ドロップでシーンやコンポーネントに使う「Project」ウィンドウ
class AssetBrowser
{
public:
	enum class AssetType { Folder, Model, AnimationModel, Texture, Audio, Scene, Other };

	struct Entry
	{
		std::string Name;	// 表示名（UTF-8）
		std::string Path;	// asset\model\box.obj のような相対パス
		AssetType   Type;
	};

private:
	static std::string        m_CurrentFolder;
	static std::vector<Entry> m_Entries;
	static bool               m_NeedRefresh;
	static float              m_IconSize;

	static void Refresh();
	static void DrawIcon(const Entry& entry, const ImVec2& min, const ImVec2& max);

public:
	static void Draw();

	static AssetType GetType(const std::string& path);
	static std::string GetStem(const std::string& path);	// フォルダと拡張子を除いた名前

	// 直前の ImGui の項目をドロップ先にする。指定した種類のアセットが落とされたら true とパスを返す
	static bool AcceptDrop(AssetType type, std::string& outPath);

	// アセットからオブジェクトを作る（Scene ビューへのドロップで使う）
	static GameObject* CreateObject(const std::string& path, const Vector3& position);
};
