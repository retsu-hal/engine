#pragma once

#include "main.h"
#include "Renderer.h"
#include "Vector3.h"
#include "GameObject.h"
class CAMERA : public GameObject
{
private:

	Vector3 m_Shake{ 0.0f, 0.0f, 0.0f };
	float m_ShakeTime = 0.0f;

public:
	Vector3 m_Target;		// 注視点
	Vector3 m_Angle;			// カメラの角度
	float  m_Fov;					// カメラの視野角
	float m_MoveSpeed;
	XMMATRIX m_ViewMatrix;		// ビュー行列
	XMMATRIX m_ProjectionMatrix;	// 射影行列

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	void SetPosition(Vector3 position) { m_Position = position; }
	Vector3 GetPosition() { return m_Position; }

	void SetTarget(Vector3 target) { m_Target = target; }
	Vector3 GetTarget() { return m_Target; }

	void SetAngle(Vector3 angle) { m_Angle = angle; }
	Vector3 GetAngle() { return m_Angle; }

	void SetFov(float fov) { m_Fov = fov; }
	float GetFov() { return m_Fov; }

	XMMATRIX GetViewMatrix() { return m_ViewMatrix; }
	XMMATRIX GetProjectionMatrix() const { return m_ProjectionMatrix; }

	Vector3 GetForward() override
	{
		Vector3 forward = m_Target - m_Position;
		forward.normalize();

		return forward;
	}

	Vector3 GetRight() override
	{
		Vector3 forward = m_Target - m_Position;
		Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
		Vector3 right = Vector3::cross(up, forward);
		right.normalize();

		return right;
	}

	void Shake(Vector3 Shake)
	{
		m_Shake = Shake;
		m_ShakeTime = 0.0f;
	}

};
