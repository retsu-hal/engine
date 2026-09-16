#pragma once
#include <vector>
#include <string>
#include "main.h"
#include "Vector3.h"

//デバッグ用の線を1フレームぶんためて、ImGuiの描画リストにまとめて流し込むクラス
//3D空間の座標を画面座標に変換して線を引くだけなので、専用のシェーダーは要らない
class Gizmo
{
private:
	struct Line
	{
		Vector3 Start;
		Vector3 End;
		ImU32   Color;
	};

	struct Label
	{
		Vector3     Position;
		std::string Text;
		ImU32       Color;
	};

	static std::vector<Line>  m_Lines;	//このフレームにためた線
	static std::vector<Label> m_Labels;	//このフレームにためた文字
	static bool  m_Enable;
	static float m_Thickness;

public:
	static void Init() {}
	static void Uninit();
	static void Draw();		//ImGui::NewFrame と ImGui::Render の間で呼ぶ

	static void SetEnable(bool enable) { m_Enable = enable; }
	static bool IsEnable() { return m_Enable; }

	static void DrawLine(const Vector3& a, const Vector3& b, const XMFLOAT4& color);
	static void DrawLabel(const Vector3& position, const char* text, const XMFLOAT4& color);
	static void DrawArc(const Vector3& center, const Vector3& axisA, const Vector3& axisB,
		float radius, float start, float end, const XMFLOAT4& color, int segments = 32);
	static void DrawCircle(const Vector3& center, float radius, const XMFLOAT4& color);
	static void DrawWireBox(const Vector3& center, const Vector3& half, const XMFLOAT4& color);
	static void DrawWireSphere(const Vector3& center, float radius, const XMFLOAT4& color);
	static void DrawWireCylinder(const Vector3& center, float radius, float halfHeight, const XMFLOAT4& color);
	static void DrawWireCapsule(const Vector3& center, float radius, float halfHeight, const XMFLOAT4& color);
};
