#pragma once
#include "GameObject.h"

class Bullet :public GameObject
{
private:
	float m_Lifetime = 1.0f; // 弾の寿命（秒）

	class Rigidbody* m_Rigidbody = nullptr;

public:
	void Init() override;
	void Update() override;
	void OnCollision(GameObject* other) override;

	void SetVelocity(const Vector3& velocity);
};

