#include "main.h"
#include "Renderer.h"
#include "Player.h"
#include "Camera.h"
#include "Manager.h"
#include "AnimationModel.h"
#include "Enemy.h"
#include "Bullet.h"
#include "Collider.h"
#include "Audio.h"
#include "Shadow.h"
#include "Rigidbody.h"

#define SHADOW_OFFSET_Y	(0.01f)		// 影を地面から浮かせる量（Zファイティング回避）


void Player::Init()
{
	m_Layer = 1;
	m_Position = { 0.0f, 1.0f, 0.0f };
	m_Scale = { 0.01f, 0.01f, 0.01f };
	m_Speed = 50.0f;
	m_jumpPower = 16.0f;

	m_AnimationModel = AddComponent<AnimationModel>(this);
	m_AnimationModel->Load("asset\\model\\Akai.fbx");
	m_AnimationModel->LoadAnimation("asset\\model\\Akai_Idle.fbx", "Idle");
	m_AnimationModel->LoadAnimation("asset\\model\\Akai_Run.fbx", "Run");

	m_AnimationName = "Idle";
	m_NextAnimationName = "Idle";
	
	CapsuleCollider*collider = AddComponent<CapsuleCollider>(this);
	collider->SetRadius(40.0f);
	collider->SetHeight(180.0f);
	collider->SetOffset({ 0.0f, 90.0f, 0.0f });

	m_Rigidbody = AddComponent<Rigidbody>(this);
	m_Rigidbody->SetGravity(40.0f);
	m_Rigidbody->SetDrag(5.0f);

	//SE
	m_JumpSE = AddComponent<Audio>(this);
	m_JumpSE->Load("asset\\audio\\wan.wav");

	m_Shadow = Manager::AddGameObject<Shadow>();
	m_Shadow->SetScale({ 1.5f, 1.5f, 1.5f });
}

void Player::Uninit()
{
	//影はManager管理なので、プレイヤーが消えるときに一緒に破棄する
	if (m_Shadow)
	{
		m_Shadow->SetDestroy();
		m_Shadow = nullptr;
	}

	GameObject::Uninit();
}

void Player::Update()
{
#if _DEBUG
	ImGui::Begin("PlayerDebug");
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	ImGui::Text("Position: (%.2f, %.2f, %.2f)", m_Position.x, m_Position.y, m_Position.z);
	ImGui::Text("Rotation: (%.2f, %.2f, %.2f)", m_Rotation.x, m_Rotation.y, m_Rotation.z);
	ImGui::Text("Scale: (%.2f, %.2f, %.2f)", m_Scale.x, m_Scale.y, m_Scale.z);
	ImGui::Text("state: %s", m_NextAnimationName.c_str());
	ImGui::Text("Hit Timer: %.2f", m_HitTimer);
	ImGui::SliderFloat("Speed", &m_Speed, 0.0f, 100.0f);
	ImGui::SliderFloat("Jump Power", &m_jumpPower, 0.0f, 100.0f);
	float gravity = m_Rigidbody->GetGravity();
	if (ImGui::SliderFloat("Gravity", &gravity, 0.0f, 100.0f)) m_Rigidbody->SetGravity(gravity);	ImGui::End();
#endif // _DEBUG


	float dt = Manager::GetDeltaTime();
	CAMERA* camera = Manager::GetGameObject<CAMERA>();
	Vector3 forward = camera->GetForward();
	Vector3 right = camera->GetRight();
	forward.y = 0.0f;
	forward.normalize();
	right.y = 0.0f;
	right.normalize();

	Vector3 moveDir = { 0.0f, 0.0f, 0.0f };

	//入力による加速
	if (Input::GetKeyPress('W')) moveDir += forward;
	if (Input::GetKeyPress('S')) moveDir -= forward;
	if (Input::GetKeyPress('D')) moveDir += right;
	if (Input::GetKeyPress('A')) moveDir -= right;

	if (moveDir.x!=0.0f || moveDir.z!=0.0f)
	{
		moveDir.normalize();
		m_Rigidbody->AddVelocity(moveDir * m_Speed * dt);

		//移動方向に回転
		m_Rotation.y = atan2f(moveDir.x, moveDir.z);
		SetAnimation("Run");
	}
	else
	{
		SetAnimation("Idle");
	}


	//ジャンプ
	if (Input::GetKeyTrigger(VK_SPACE)&&m_Rigidbody->IsGrounded())
	{
		m_Rigidbody->AddVelocity({ 0.0f, m_jumpPower, 0.0f });
		m_JumpSE->Play();		//ジャンプSE
	}

	GameObject::Update();

	//無敵時間
	if (m_HitTimer > 0.0f) m_HitTimer -= dt;
	if (m_HitTimer < 0.0f) m_HitTimer = 0.0f;


	//弾発射
	if (Input::GetMousePress(Input::MOUSE_LEFT))
	{
		Vector3 spawnPos = m_Position + GetForward() * 0.5f + Vector3(0.0f, 1.0f, 0.0f);
		
		Bullet* bullet = Manager::AddGameObject<Bullet>();
		bullet->SetPosition(spawnPos);
		bullet->SetVelocity(GetForward()*10.0f);
	}

	//影移動（地面と同一平面だとZファイティングするので少し浮かせる）
	if (m_Shadow)
	{
		Vector3 ShadowPos = m_Position;
		ShadowPos.y =  SHADOW_OFFSET_Y;
		m_Shadow->SetPosition(ShadowPos);
	}
	
	m_AnimationFrame++;
	m_NextAnimationFrame++;

	m_Blend += 0.1f;
	if (m_Blend > 1.0f) m_Blend = 1.0f;

}

void Player::Draw()
{
	// 点滅
	if (m_HitTimer > 0.0f)
	{
		if (((int)(m_HitTimer * 10.0f)) % 2 == 0)
			return;
	}

	m_AnimationModel->Update(m_AnimationName.c_str(), m_AnimationFrame, m_NextAnimationName.c_str(), m_NextAnimationFrame, m_Blend);
	
	GameObject::Draw();
}

void Player::SetAnimation(const char* AnimationName)
{
	if (m_NextAnimationName != AnimationName)
	{
		// 次のアニメーションを設定
		m_AnimationName = m_NextAnimationName;
		m_AnimationFrame = m_NextAnimationFrame;

		m_NextAnimationName = AnimationName;
		m_NextAnimationFrame = 0;

		m_Blend = 0.0f;
	}
}

void Player::OnCollision(GameObject* other)
{
	if (dynamic_cast<Enemy*>(other) && m_HitTimer <= 0.0f) m_HitTimer = 1.0f;
}

