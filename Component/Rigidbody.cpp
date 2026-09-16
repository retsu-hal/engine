

/*==============================================================================

[Rigidbody.cpp]
														Author :Watanabe Retsu
														Date   :2026/09/11
--------------------------------------------------------------------------------

==============================================================================*/

//==============================================================================
//インクルード
//==============================================================================
#include "main.h"
#include "JsonUtil.h"
#include "Registry.h"
#include "Renderer.h"
#include "Manager.h"
#include "GameObject.h"
#include "MeshField.h"
#include "Rigidbody.h"

//==============================================================================
//更新処理
//==============================================================================
void Rigidbody::Update()
{
	float dt = Manager::GetDeltaTime();

	//重力
	if (m_UseGravity) m_Velocity.y -= m_Gravity * dt;

	//移動
	Vector3 pos = m_GameObject->GetPosition() + m_Velocity * dt;

	//水平方向の減速
	m_Velocity.x -= m_Velocity.x * m_Drag * dt;
	m_Velocity.z -= m_Velocity.z * m_Drag * dt;

	//地面判定（接地状態は毎フレーム計算し直す）
	m_IsGrounded = false;
	if (m_UseGround)
	{
		MeshField* field = Manager::GetGameObject<MeshField>();
		m_GroundHeight = field ? field->GetHeight(pos) : 0.0f;

		if (pos.y < m_GroundHeight)
		{
			pos.y = m_GroundHeight;
			if (m_Velocity.y < 0.0f) m_Velocity.y = 0.0f;
			m_IsGrounded = true;
		}
	}
	m_GameObject->SetPosition(pos);
}

void Rigidbody::OnPushed(const Vector3& push)
{
	//上に押し戻された：何かの上に乗った
	if (push.y > 0.0f && m_Velocity.y <= 0.0f)
	{
		m_Velocity.y = 0.0f;
		m_IsGrounded = true;
	}
	//下に押し戻された：頭をぶつけた
	else if (push.y < 0.0f && m_Velocity.y > 0.0f)
	{
		m_Velocity.y = 0.0f;
	}

	//横に押し戻された：壁に向かう速度だけ消す（壁に沿って滑れる）
	float len = sqrtf(push.x * push.x + push.z * push.z);
	if (len > 0.0001f)
	{
		float nx = push.x / len;
		float nz = push.z / len;
		float dot = m_Velocity.x * nx + m_Velocity.z * nz;
		if (dot < 0.0f)
		{
			m_Velocity.x -= nx * dot;
			m_Velocity.z -= nz * dot;
		}
	}
}

void Rigidbody::OnInspectorGUI()
{
	ImGui::DragFloat("Gravity", &m_Gravity, 0.1f);
	ImGui::DragFloat("Drag", &m_Drag, 0.1f);
	ImGui::Checkbox("Use Gravity", &m_UseGravity);
	ImGui::SameLine();
	ImGui::Checkbox("Use Ground", &m_UseGround);
	ImGui::Text("Velocity: (%.2f, %.2f, %.2f)  Grounded: %s", m_Velocity.x, m_Velocity.y, m_Velocity.z, m_IsGrounded ? "true" : "false");
}

void Rigidbody::Serialize(nlohmann::json& data) const
{
	data["gravity"] = m_Gravity;
	data["drag"] = m_Drag;
	data["useGravity"] = m_UseGravity;
	data["useGround"] = m_UseGround;
}

void Rigidbody::Deserialize(const nlohmann::json& data)
{
	JsonRead(data, "gravity", m_Gravity);
	JsonRead(data, "drag", m_Drag);
	JsonRead(data, "useGravity", m_UseGravity);
	JsonRead(data, "useGround", m_UseGround);
}

REGISTER_COMPONENT(Rigidbody)
