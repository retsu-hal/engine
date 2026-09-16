#include "main.h"
#include "Renderer.h"
#include "JsonUtil.h"
#include "Registry.h"
#include "Manager.h"
#include "GameObject.h"
#include "EnemyController.h"
#include "PlayerController.h"
#include "ModelRenderer.h"
#include "Camera.h"
#include "Prefabs.h"

#define FLASH_DURATION	(0.05f)		// 被弾時に白く光る時間（秒）

void EnemyController::Start()
{
	m_ModelRenderer = m_GameObject->GetComponent<ModelRenderer>();
}

void EnemyController::Update()
{
	float dt = Manager::GetDeltaTime();

	//------------------------------------------------------------
	// プレイヤー追従（XZ平面のみ）
	//------------------------------------------------------------
	if (PlayerController* player = Manager::FindComponent<PlayerController>())
	{
		Vector3 position = m_GameObject->GetPosition();
		Vector3 direction = player->GetGameObject()->GetWorldPosition() - position;
		direction.y = 0.0f;

		float length = direction.length();
		if (length > 0.01f)
		{
			direction = direction / length;
			m_GameObject->SetPosition(position + direction * m_Speed * dt);

			Vector3 rotation = m_GameObject->GetRotation();
			rotation.y = atan2f(-direction.x, -direction.z);
			m_GameObject->SetRotation(rotation);
		}
	}

	//ヒットフラッシュ
	if (m_FlashTime > 0.0f) m_FlashTime -= dt;
	if (m_ModelRenderer) m_ModelRenderer->SetFlash(m_FlashTime > 0.0f);
}

void EnemyController::AddDamage(int damage)
{
	if (m_GameObject->IsDestroyed()) return;

	m_Life -= damage;
	m_FlashTime = FLASH_DURATION;

	if (m_Life <= 0)
	{
		m_GameObject->SetDestroy();
		Prefabs::CreateExplosion(m_GameObject->GetWorldPosition(), { 2.0f, 2.0f, 2.0f });

		//既存のカメラを取得して揺らす
		CAMERA* camera = Manager::GetGameObject<CAMERA>();
		if (camera)	camera->Shake({ 0.0f, 1.0f, 0.0f });
	}
}

void EnemyController::OnInspectorGUI()
{
	ImGui::DragFloat("Speed", &m_Speed, 0.1f);
	ImGui::DragInt("Life", &m_Life);
}

void EnemyController::Serialize(nlohmann::json& data) const
{
	data["speed"] = m_Speed;
	data["life"] = m_Life;
}

void EnemyController::Deserialize(const nlohmann::json& data)
{
	JsonRead(data, "speed", m_Speed);
	JsonRead(data, "life", m_Life);
}

REGISTER_COMPONENT(EnemyController)
