#pragma once
#include "EngineAPI.h"
#include "Component.h"

class BillboardRenderer;

class ENGINE_API SpriteAnimation : public Component
{
private:
	BillboardRenderer* m_Renderer = nullptr;
	int   m_DivX = 1;
	int   m_DivY = 1;
	int   m_FrameCount = 1;
	float m_Fps = 60.0f;
	float m_Time = 0.0f;
	bool  m_Loop = false;	// false: 最後まで再生したらオブジェクトを破棄

	void SetFrame(int frame);

public:
	using Component::Component;

	void Setup(BillboardRenderer* renderer, int divX, int divY, int frameCount, float fps, bool loop = false);
	void Start() override;
	void Update() override;

	void OnInspectorGUI() override;
	void Serialize(nlohmann::json& data) const override;
	void Deserialize(const nlohmann::json& data) override;
};