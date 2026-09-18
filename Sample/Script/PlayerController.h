#pragma once
#include <string>
#include "Component.h"

class PlayerController : public Component
{
private:
	class Rigidbody*      m_Rigidbody = nullptr;
	class AnimationModel* m_AnimationModel = nullptr;
	class Audio*          m_JumpSE = nullptr;

	float m_Speed = 50.0f;
	float m_JumpPower = 16.0f;
	float m_HitTimer = 0.0f;	// 無敵時間
	bool  m_Blinking = false;	// 点滅でモデルの Enabled を操作中か

	// アニメーション（前のアニメから次のアニメへブレンドする）
	std::string m_AnimationName = "Idle";
	int         m_AnimationFrame = 0;
	std::string m_NextAnimationName = "Idle";
	int         m_NextAnimationFrame = 0;
	float       m_Blend = 1.0f;

	void SetAnimation(const char* animationName);

public:
	using Component::Component;

	void Start() override;
	void Update() override;
	void OnCollision(GameObject* other) override;
	void OnInspectorGUI() override;
	void Serialize(nlohmann::json& data) const override;
	void Deserialize(const nlohmann::json& data) override;
};
