#include "main.h"
#include "Renderer.h"
#include "JsonUtil.h"
#include "Registry.h"
#include "Manager.h"
#include "GameObject.h"
#include "BulletController.h"
#include "EnemyController.h"
#include "Rigidbody.h"
#include "Score.h"

void BulletController::Start()
{
	m_Rigidbody = m_GameObject->GetComponent<Rigidbody>();
}

void BulletController::Update()
{
	m_Rigidbody = m_GameObject->GetComponent<Rigidbody>();

	m_Lifetime -= Manager::GetDeltaTime();
	if (m_Lifetime <= 0.0f)
	{
		m_GameObject->SetDestroy();
	}
}

void BulletController::OnCollision(GameObject* other)
{
	if (m_GameObject->IsDestroyed()) return;	// 同じフレームに複数と当たったとき、二重に処理しない

	if (EnemyController* enemy = other->GetComponent<EnemyController>())
	{
		enemy->AddDamage(1);
		m_GameObject->SetDestroy();

		//スコア加算
		for (Score* score : Manager::GetGameObjects<Score>())	score->AddScore(100);
	}
	else if (other->GetComponent<BulletController>() == nullptr)
	{
		m_GameObject->SetDestroy();
	}
}

void BulletController::OnInspectorGUI()
{
	ImGui::DragFloat("Lifetime", &m_Lifetime, 0.01f);
}

void BulletController::Serialize(nlohmann::json& data) const
{
	data["lifetime"] = m_Lifetime;
}

void BulletController::Deserialize(const nlohmann::json& data)
{
	JsonRead(data, "lifetime", m_Lifetime);
}

REGISTER_COMPONENT(BulletController)
