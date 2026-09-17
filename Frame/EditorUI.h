#pragma once
#include <string>
#include "Vector3.h"

//=============================================================
// エディタの各ウィンドウで共通して使う ImGui の部品
// （アイコンボタン・縦横比を保つ計算・Vector3 の行・コンポーネントの見出し）
//=============================================================
namespace EditorUI
{
	// 図形で描くアイコン（フォントに記号がなくても表示できる）
	enum class Icon
	{
		Play, Pause, Stop,			// ツールバー
		Move, Rotate, Scale,		// ギズモの操作
		Local, World,				// ギズモの座標系
		Grid,						// グリッド表示
	};

	// アイコンのボタン。widthScale はボタンの横幅（行の高さの何倍か）
	bool IconButton(const char* id, Icon icon, bool active, const char* tooltip, float widthScale = 1.3f);

	// avail に収まる範囲で、縦横比 aspect を保った大きさ
	ImVec2 FitAspect(const ImVec2& avail, float aspect);

	// "BoxCollider" -> "Box Collider"（Unity の ObjectNames.NicifyVariableName と同じ考え方）
	std::string NicifyName(const std::string& name);

	// 種類ごとのアイコン（1文字）と色
	void GetComponentIcon(const std::string& typeName, const char*& icon, ImVec4& color);

	// CollapsingHeader の直後に呼ぶ：見出しの上にアイコン・有効チェック・名前を重ねて描き、
	// 右端の「…」ボタンを置く位置へ移動する
	void ComponentHeaderLabel(const char* label, const char* icon, const ImVec4& iconColor, bool* enabled);

	// Unity 風の Vector3 の行：「Position   [X ___] [Y ___] [Z ___]」
	// X/Y/Z の色付きラベルを左右にドラッグしても値が変わる。右クリックで 0（Scale は 1）に戻す
	bool Vector3Field(const char* label, Vector3& value, float speed, float labelWidth);
}
