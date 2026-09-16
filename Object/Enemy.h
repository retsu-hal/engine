#pragma once
#include "GameObject.h"
class Enemy :public GameObject
{
private:
	class ModelRenderer* m_ModelRenderer;
	class Rigidbody* m_Rigidbody;

	float m_Speed = 3.0f;

	int m_Life = 0;

	bool m_Flash = false;
	float m_FlashTime = 0.0f;

public:
	void Init() override;
	void Update() override;

	void AddDamage(int Damage);
};

