/*==============================================================================

[Enemy.cpp]
														Author :Watanabe Retsu
														Date   :
--------------------------------------------------------------------------------

==============================================================================*/

//==============================================================================
//インクルード
//==============================================================================
#include "main.h"
#include "Renderer.h"
#include "Manager.h"
#include "Enemy.h"
#include "ModelRenderer.h"
#include "Player.h"
#include "Explosion.h"
#include "Camera.h"
#include "Collider.h"
#include "Rigidbody.h"
#include "MeshField.h"

//==============================================================================
//マクロ宣言
//==============================================================================
#define FLASH_DURATION	(0.05f)		// 被弾時に白く光る時間（秒）

//==============================================================================
//プロトタイプ宣言
//==============================================================================

//==============================================================================
//グローバル変数
//==============================================================================

//==============================================================================
//初期化処理
//==============================================================================
void Enemy::Init()
{
	m_Layer = 1;
	m_Life = 5;
	m_Flash = false;

	m_Position = { 0.0f, 5.0f, 0.0f };
	m_Scale = { 1.0f, 1.0f, 1.0f };

	m_ModelRenderer = AddComponent<ModelRenderer>(this);
	m_ModelRenderer->Load("asset\\model\\player.obj");


	CapsuleCollider* collider = AddComponent<CapsuleCollider>(this);
	collider->SetRadius(0.5f);
	collider->SetHeight(2.0f);
	collider->SetOffset({ 0.0f, 1.0f, 0.0f });

	m_Rigidbody = AddComponent<Rigidbody>(this);
	m_Rigidbody->SetGravity(40.0f);
	m_Rigidbody->SetDrag(5.0f);


}

//==============================================================================
//更新処理
//==============================================================================
void Enemy::Update()
{
	float dt = Manager::GetDeltaTime();

	//------------------------------------------------------------
	// プレイヤー追従（XZ平面のみ。ジャンプに繋げない）
	//------------------------------------------------------------
	auto players = Manager::GetGameObjects<Player>();
	if (!players.empty())
	{
		Player* player = players[0];

		Vector3 direction = player->GetPosition() - m_Position;
		direction.y = 0.0f;

		float length = direction.length();
		if (length > 0.01f)
		{
			direction = direction / length;
			m_Position += direction * m_Speed * dt;
			m_Rotation.y = atan2f(-direction.x, -direction.z);
		}
	}

	//ヒットフラッシュ
	if (m_FlashTime > 0.0f)
	{
		m_FlashTime -= dt;
		if (m_FlashTime <= 0.0f)	m_Flash = false;
	}

	m_ModelRenderer->SetFlash(m_Flash);

	GameObject::Update();
}

void Enemy::AddDamage(int Damage)
{
	m_Life -= Damage;

	m_Flash = true;
	m_FlashTime = FLASH_DURATION;

	if (m_Life <= 0)
	{
		SetDestroy();
		Explosion* explosion = Manager::AddGameObject<Explosion>();
		explosion->SetPosition(m_Position);
		explosion->SetScale({ 2.0f,2.0f,2.0f });

		//既存のカメラを取得して揺らす（AddGameObjectだとカメラが増えてしまう）
		CAMERA* camera = Manager::GetGameObject<CAMERA>();
		if (camera)	camera->Shake({ 0.0f,1.0f,0.0f });
	}
}
