#include "main.h"
#include "Manager.h"
#include "Camera.h"
#include "Gizmo.h"

static const Vector3 AXIS_X(1.0f, 0.0f, 0.0f);
static const Vector3 AXIS_Y(0.0f, 1.0f, 0.0f);
static const Vector3 AXIS_Z(0.0f, 0.0f, 1.0f);

std::vector<Gizmo::Line>  Gizmo::m_Lines;
std::vector<Gizmo::Label> Gizmo::m_Labels;
#if _DEBUG
bool Gizmo::m_Enable = true;
#else
bool Gizmo::m_Enable = false;
#endif
float Gizmo::m_Thickness = 1.5f;

static ImU32 ToColor(const XMFLOAT4& c)
{
	return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, c.w));
}

//ワールド座標 → クリップ座標
static XMVECTOR ToClip(const Vector3& p, const XMMATRIX& viewProj)
{
	return XMVector4Transform(XMVectorSet(p.x, p.y, p.z, 1.0f), viewProj);
}

//クリップ座標 → 画面座標（ピクセル）
static ImVec2 ToScreen(XMVECTOR clip, const ImGuiViewport* vp)
{
	float w = XMVectorGetW(clip);
	float x = XMVectorGetX(clip) / w;
	float y = XMVectorGetY(clip) / w;
	return ImVec2(vp->Pos.x + (x * 0.5f + 0.5f) * vp->Size.x,
		vp->Pos.y + (-y * 0.5f + 0.5f) * vp->Size.y);
}

//=============================================================
// 終了
//=============================================================
void Gizmo::Uninit()
{
	m_Lines.clear();
	m_Labels.clear();
}

//=============================================================
// ためた線と文字を ImGui に描かせる
//=============================================================
void Gizmo::Draw()
{
#if _DEBUG
	ImGui::Begin("Gizmo");
	ImGui::Checkbox("Show (F1)", &m_Enable);
	ImGui::SliderFloat("Thickness", &m_Thickness, 1.0f, 5.0f);
	ImGui::Text("Lines: %d", (int)m_Lines.size());
	ImGui::End();
#endif

	CAMERA* camera = Manager::GetGameObject<CAMERA>();
	if (m_Enable && camera)
	{
		XMMATRIX viewProj = camera->GetViewMatrix() * camera->GetProjectionMatrix();
		ImDrawList* drawList = ImGui::GetBackgroundDrawList();	//ImGuiのウィンドウより奥に描く
		const ImGuiViewport* vp = ImGui::GetMainViewport();

		for (const Line& line : m_Lines)
		{
			XMVECTOR a = ToClip(line.Start, viewProj);
			XMVECTOR b = ToClip(line.End, viewProj);
			float za = XMVectorGetZ(a);
			float zb = XMVectorGetZ(b);

			//カメラの後ろ（ニアクリップより手前）にある部分を切り取る
			if (za < 0.0f && zb < 0.0f) continue;
			if (za < 0.0f)      a = XMVectorLerp(a, b, za / (za - zb));
			else if (zb < 0.0f) b = XMVectorLerp(b, a, zb / (zb - za));

			drawList->AddLine(ToScreen(a, vp), ToScreen(b, vp), line.Color, m_Thickness);
		}

		for (const Label& label : m_Labels)
		{
			XMVECTOR p = ToClip(label.Position, viewProj);
			if (XMVectorGetZ(p) < 0.0f) continue;	//カメラの後ろ
			drawList->AddText(ToScreen(p, vp), label.Color, label.Text.c_str());
		}
	}

	m_Lines.clear();
	m_Labels.clear();
}

//=============================================================
// 線・文字
//=============================================================
void Gizmo::DrawLine(const Vector3& a, const Vector3& b, const XMFLOAT4& color)
{
	if (!m_Enable) return;
	m_Lines.push_back({ a, b, ToColor(color) });
}

void Gizmo::DrawLabel(const Vector3& position, const char* text, const XMFLOAT4& color)
{
	if (!m_Enable) return;
	m_Labels.push_back({ position, text, ToColor(color) });
}

//=============================================================
// 円弧
// axisA を角度0、axisB を角度90度の向きとして、start〜end まで線をつなぐ
//=============================================================
void Gizmo::DrawArc(const Vector3& center, const Vector3& axisA, const Vector3& axisB,
	float radius, float start, float end, const XMFLOAT4& color, int segments)
{
	if (!m_Enable) return;
	if (segments < 1) segments = 1;

	Vector3 prev = center + axisA * (cosf(start) * radius) + axisB * (sinf(start) * radius);

	for (int i = 1; i <= segments; i++)
	{
		float t = start + (end - start) * ((float)i / (float)segments);
		Vector3 next = center + axisA * (cosf(t) * radius) + axisB * (sinf(t) * radius);
		DrawLine(prev, next, color);
		prev = next;
	}
}

//XZ平面の円（真上から見た形）
void Gizmo::DrawCircle(const Vector3& center, float radius, const XMFLOAT4& color)
{
	DrawArc(center, AXIS_X, AXIS_Z, radius, 0.0f, XM_2PI, color);
}

//=============================================================
// 図形
//=============================================================
void Gizmo::DrawWireBox(const Vector3& center, const Vector3& half, const XMFLOAT4& color)
{
	if (!m_Enable) return;

	//ビット0=X、ビット1=Y、ビット2=Z が「＋側かどうか」を表す8頂点
	Vector3 corner[8];
	for (int i = 0; i < 8; i++)
	{
		corner[i] = Vector3(
			center.x + ((i & 1) ? half.x : -half.x),
			center.y + ((i & 2) ? half.y : -half.y),
			center.z + ((i & 4) ? half.z : -half.z));
	}

	//1ビットだけ違う頂点同士が辺になる（重複しないよう＋側へ向かう分だけ引く）
	for (int i = 0; i < 8; i++)
	{
		for (int bit = 1; bit < 8; bit <<= 1)
		{
			if (!(i & bit)) DrawLine(corner[i], corner[i | bit], color);
		}
	}
}

void Gizmo::DrawWireSphere(const Vector3& center, float radius, const XMFLOAT4& color)
{
	DrawArc(center, AXIS_X, AXIS_Z, radius, 0.0f, XM_2PI, color);	//横向きの輪
	DrawArc(center, AXIS_X, AXIS_Y, radius, 0.0f, XM_2PI, color);	//縦向きの輪
	DrawArc(center, AXIS_Z, AXIS_Y, radius, 0.0f, XM_2PI, color);	//縦向きの輪
}

void Gizmo::DrawWireCylinder(const Vector3& center, float radius, float halfHeight, const XMFLOAT4& color)
{
	Vector3 top = center + AXIS_Y * halfHeight;
	Vector3 bottom = center - AXIS_Y * halfHeight;

	DrawCircle(top, radius, color);
	DrawCircle(bottom, radius, color);

	//側面の柱4本
	DrawLine(top + AXIS_X * radius, bottom + AXIS_X * radius, color);
	DrawLine(top - AXIS_X * radius, bottom - AXIS_X * radius, color);
	DrawLine(top + AXIS_Z * radius, bottom + AXIS_Z * radius, color);
	DrawLine(top - AXIS_Z * radius, bottom - AXIS_Z * radius, color);
}

void Gizmo::DrawWireCapsule(const Vector3& center, float radius, float halfHeight, const XMFLOAT4& color)
{
	Vector3 top = center + AXIS_Y * halfHeight;		//上の球の中心
	Vector3 bottom = center - AXIS_Y * halfHeight;	//下の球の中心

	//丸い部分のつなぎ目
	DrawCircle(top, radius, color);
	DrawCircle(bottom, radius, color);

	//上下の半球（縦向きの半円を2枚ずつ）
	DrawArc(top, AXIS_X, AXIS_Y, radius, 0.0f, XM_PI, color);
	DrawArc(top, AXIS_Z, AXIS_Y, radius, 0.0f, XM_PI, color);
	DrawArc(bottom, AXIS_X, AXIS_Y, radius, XM_PI, XM_2PI, color);
	DrawArc(bottom, AXIS_Z, AXIS_Y, radius, XM_PI, XM_2PI, color);

	//側面の柱4本
	DrawLine(top + AXIS_X * radius, bottom + AXIS_X * radius, color);
	DrawLine(top - AXIS_X * radius, bottom - AXIS_X * radius, color);
	DrawLine(top + AXIS_Z * radius, bottom + AXIS_Z * radius, color);
	DrawLine(top - AXIS_Z * radius, bottom - AXIS_Z * radius, color);
}
