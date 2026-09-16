/*==============================================================================

[Bullet.cpp]
														Author :Watanabe Retsu
														Date   :
--------------------------------------------------------------------------------

==============================================================================*/

//==============================================================================
//インクルード
//==============================================================================
#include "main.h"
#include "Manager.h"
#include "Renderer.h"
#include "Bullet.h"
#include "Score.h"
#include "Enemy.h"
#include "ModelRenderer.h"
#include "Collider.h"
#include "Rigidbody.h"

//==============================================================================
//初期化処理
//==============================================================================
void Bullet::Init()
{
	m_Layer = 1;
	AddComponent<ModelRenderer>(this)->Load("asset\\model\\bullet.obj");

	SphereCollider* collider = AddComponent<SphereCollider>(this);
	collider->SetRadius(0.3f);
	collider->SetTrigger(true);

	m_Rigidbody = AddComponent<Rigidbody>(this);
	m_Rigidbody->SetUseGravity(false);
	m_Rigidbody->SetUseGround(false);

}

//==============================================================================
//更新処理
//==============================================================================
void Bullet::Update()
{
	float dt = Manager::GetDeltaTime();

	m_Position += m_Rigidbody->GetVelocity() * dt;

	m_Lifetime -= dt;
	if (m_Lifetime <= 0.0f)
	{
		SetDestroy();
	}

	GameObject::Update();
}

void Bullet::OnCollision(GameObject* other)
{
	if (Enemy* enemy = dynamic_cast<Enemy*>(other))
	{
		enemy->AddDamage(1);
		SetDestroy();

		//スコア加算
		for(Score* score : Manager::GetGameObjects<Score>())	score->AddScore(100);
	}
	else if(!dynamic_cast<Bullet*>(other))
	{
		SetDestroy();
	}
}

void Bullet::SetVelocity(const Vector3& velocity)
{
	m_Rigidbody->SetVelocity(velocity);
}
