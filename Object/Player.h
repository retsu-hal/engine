#pragma once

#include <string>

#include "Vector3.h"
#include "GameObject.h"

class Player : public GameObject
{
private:
	class Rigidbody* m_Rigidbody = nullptr;
	float m_Speed ;
	float m_jumpPower ;
	float m_MoveAnimation = 0.0f;
	float m_HitTimer = 0.0f;
	class Audio* m_JumpSE = nullptr;
	class Shadow* m_Shadow = nullptr;

	class AnimationModel* m_AnimationModel;
	int m_AnimationFrame = 0;
	std::string m_AnimationName; // 現在のアニメーション名

	int m_NextAnimationFrame = 0;
	std::string m_NextAnimationName; // 次のアニメーション名

	float m_Blend = 0.0f; // アニメーションの補間値

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	void SetAnimation(const char* AnimationName);

	void OnCollision(GameObject* other) override;

};



