#pragma once
#include "Component.h"

class EnemyController : public Component
{
private:
	class ModelRenderer* m_ModelRenderer = nullptr;

	float m_Speed = 3.0f;
	int   m_Life = 5;
	float m_FlashTime = 0.0f;

public:
	using Component::Component;

	void Start() override;
	void Update() override;
	void OnInspectorGUI() override;
	void Serialize(nlohmann::json& data) const override;
	void Deserialize(const nlohmann::json& data) override;

	void AddDamage(int damage);
};
