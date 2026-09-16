#include "main.h"
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

void SpriteAnimation::Update()
{
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