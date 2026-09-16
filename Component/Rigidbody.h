#pragma once
#include "Component.h"
#include "Vector3.h"

class Rigidbody : public Component
{
private:
	Vector3 m_Velocity{0.0f, 0.0f, 0.0f};
	float m_Gravity = 40.0f;
	float m_Drag = 0.0f;			
	bool  m_UseGravity = true;
	bool  m_UseGround = true;		
	bool  m_IsGrounded = false;		
	float m_GroundHeight = 0.0f;

public:
	using Component::Component;

	void Update() override;
	void OnPushed(const Vector3& push);	

	void SetVelocity(const Vector3& velocity) { m_Velocity = velocity; }
	void AddVelocity(const Vector3& velocity) { m_Velocity += velocity; }
	Vector3 GetVelocity() const { return m_Velocity; }

	void  SetGravity(float gravity) { m_Gravity = gravity; }
	float GetGravity() const { return m_Gravity; }
	void  SetDrag(float drag) { m_Drag = drag; }
	void  SetUseGravity(bool use) { m_UseGravity = use; }
	void  SetUseGround(bool use) { m_UseGround = use; }

	bool  IsGrounded() const { return m_IsGrounded; }
	float GetGroundHeight() const { return m_GroundHeight; }
};

