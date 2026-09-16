#pragma once
#include "Component.h"

class BulletController : public Component
{
private:
	float m_Lifetime = 1.0f;	// 弾の寿命（秒）
	class Rigidbody* m_Rigidbody = nullptr;

public:
	using Component::Component;

	void Start() override;
	void Update() override;
	void OnCollision(GameObject* other) override;
	void OnInspectorGUI() override;
	void Serialize(nlohmann::json& data) const override;
	void Deserialize(const nlohmann::json& data) override;
};
