#include "main.h"
#include "JsonUtil.h"
#include "Registry.h"
#include "Manager.h"
#include "GameObject.h"
#include "BillboardRenderer.h"
#include "SpriteAnimation.h"

void SpriteAnimation::Setup(BillboardRenderer* renderer, int divX, int divY, int frameCount, float fps, bool loop)
{
	m_Renderer = renderer;
	m_DivX = divX;
	m_DivY = divY;
	m_FrameCount = frameCount;
	m_Fps = fps;
	m_Loop = loop;
	m_Time = 0.0f;

	SetFrame(0);	//最初の描画から正しいコマを表示する
}

// ファイルから読み込んだときは Setup が呼ばれないので、同じオブジェクトの BillboardRenderer を使う
void SpriteAnimation::Start()
{
	if (m_Renderer == nullptr) m_Renderer = m_GameObject->GetComponent<BillboardRenderer>();
	if (m_Renderer) SetFrame(0);
}

void SpriteAnimation::Update()
{
	m_Renderer = m_GameObject->GetComponent<BillboardRenderer>();	// 外されても落ちないよう毎フレーム探し直す
	if (m_Renderer == nullptr) return;

	m_Time += Manager::GetDeltaTime();
	int frame = (int)(m_Time * m_Fps);

	if (frame >= m_FrameCount)
	{
		if (!m_Loop)
		{
			m_GameObject->SetDestroy();
			return;
		}
		frame %= m_FrameCount;
	}

	SetFrame(frame);
}

void SpriteAnimation::SetFrame(int frame)
{
	float w = 1.0f / m_DivX;
	float h = 1.0f / m_DivY;
	m_Renderer->SetUV(w * (frame % m_DivX), h * (frame / m_DivX), w, h);
}

void SpriteAnimation::OnInspectorGUI()
{
	ImGui::DragInt("Div X", &m_DivX, 1, 1, 64);
	ImGui::DragInt("Div Y", &m_DivY, 1, 1, 64);
	ImGui::DragInt("Frames", &m_FrameCount, 1, 1, 4096);
	ImGui::DragFloat("FPS", &m_Fps, 0.5f, 1.0f, 240.0f);
	ImGui::Checkbox("Loop", &m_Loop);
}

void SpriteAnimation::Serialize(nlohmann::json& data) const
{
	data["divX"] = m_DivX;
	data["divY"] = m_DivY;
	data["frameCount"] = m_FrameCount;
	data["fps"] = m_Fps;
	data["loop"] = m_Loop;
}

void SpriteAnimation::Deserialize(const nlohmann::json& data)
{
	JsonRead(data, "divX", m_DivX);
	JsonRead(data, "divY", m_DivY);
	JsonRead(data, "frameCount", m_FrameCount);
	JsonRead(data, "fps", m_Fps);
	JsonRead(data, "loop", m_Loop);
}

REGISTER_COMPONENT(SpriteAnimation)
