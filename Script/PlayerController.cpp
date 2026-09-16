#include "main.h"
#include "Renderer.h"
#include "Manager.h"
#include "GameObject.h"
#include "PlayerController.h"
#include "EnemyController.h"
#include "AnimationModel.h"
#include "Rigidbody.h"
#include "Audio.h"
#include "Camera.h"
#include "Prefabs.h"

// 他のコンポーネントは Prefabs で後から付くので、Init ではなく Start で探す
void PlayerController::Start()
{
	m_Rigidbody = m_GameObject->GetComponent<Rigidbody>();
	m_AnimationModel = m_GameObject->GetComponent<AnimationModel>();
	m_JumpSE = m_GameObject->GetComponent<Audio>();
}

void PlayerController::Update()
{
	float dt = Manager::GetDeltaTime();

	//------------------------------------------------------------
	// 移動（カメラの向き基準）
	//------------------------------------------------------------
	Vector3 forward = { 0.0f, 0.0f, 1.0f };
	Vector3 right = { 1.0f, 0.0f, 0.0f };
	if (CAMERA* camera = Manager::GetGameObject<CAMERA>())
	{
		forward = camera->GetForward();
		right = camera->GetRight();
	}
	forward.y = 0.0f;
	forward.normalize();
	right.y = 0.0f;
	right.normalize();

	Vector3 moveDir = { 0.0f, 0.0f, 0.0f };
	if (Input::GetKeyPress('W')) moveDir += forward;
	if (Input::GetKeyPress('S')) moveDir -= forward;
	if (Input::GetKeyPress('D')) moveDir += right;
	if (Input::GetKeyPress('A')) moveDir -= right;

	if (moveDir.x != 0.0f || moveDir.z != 0.0f)
	{
		moveDir.normalize();
		if (m_Rigidbody) m_Rigidbody->AddVelocity(moveDir * m_Speed * dt);

		//移動方向に回転
		Vector3 rotation = m_GameObject->GetRotation();
		rotation.y = atan2f(moveDir.x, moveDir.z);
		m_GameObject->SetRotation(rotation);

		SetAnimation("Run");
	}
	else
	{
		SetAnimation("Idle");
	}

	//------------------------------------------------------------
	// ジャンプ
	//------------------------------------------------------------
	if (Input::GetKeyTrigger(VK_SPACE) && m_Rigidbody && m_Rigidbody->IsGrounded())
	{
		m_Rigidbody->AddVelocity({ 0.0f, m_JumpPower, 0.0f });
		if (m_JumpSE) m_JumpSE->Play();
	}

	//------------------------------------------------------------
	// 弾発射
	//------------------------------------------------------------
	if (Input::GetMousePress(Input::MOUSE_LEFT))
	{
		Vector3 playerForward = m_GameObject->GetForward();
		Vector3 spawnPos = m_GameObject->GetWorldPosition() + playerForward * 0.5f + Vector3(0.0f, 1.0f, 0.0f);
		// 旧 Bullet クラスは自前の移動と Rigidbody で二重に動いていたので、同じ速さになるよう 20 にしている
		Prefabs::CreateBullet(spawnPos, playerForward * 20.0f);
	}

	//------------------------------------------------------------
	// 無敵時間と点滅（点滅はモデルの Enabled を切り替える）
	//------------------------------------------------------------
	if (m_HitTimer > 0.0f) m_HitTimer -= dt;
	if (m_HitTimer < 0.0f) m_HitTimer = 0.0f;
	bool  m_Blinking = false;

	// 点滅中だけ Enabled を操作する（常に上書きすると Inspector から切り替えられなくなる）
	if (m_AnimationModel)
	{
		if (m_HitTimer > 0.0f)
		{
			bool visible = ((int)(m_HitTimer * 10.0f)) % 2 != 0;
			m_AnimationModel->SetEnabled(visible);
			m_Blinking = true;
		}
		else if (m_Blinking)
		{
			m_AnimationModel->SetEnabled(true);	// 点滅が終わったら表示に戻す
			m_Blinking = false;
		}
	}

	//------------------------------------------------------------
	// アニメーション
	//------------------------------------------------------------
	m_AnimationFrame++;
	m_NextAnimationFrame++;

	m_Blend += 0.1f;
	if (m_Blend > 1.0f) m_Blend = 1.0f;

	if (m_AnimationModel)
	{
		m_AnimationModel->Update(m_AnimationName.c_str(), m_AnimationFrame,
			m_NextAnimationName.c_str(), m_NextAnimationFrame, m_Blend);
	}
}

void PlayerController::SetAnimation(const char* animationName)
{
	if (m_NextAnimationName != animationName)
	{
		m_AnimationName = m_NextAnimationName;
		m_AnimationFrame = m_NextAnimationFrame;

		m_NextAnimationName = animationName;
		m_NextAnimationFrame = 0;

		m_Blend = 0.0f;
	}
}

void PlayerController::OnCollision(GameObject* other)
{
	if (other->GetComponent<EnemyController>() && m_HitTimer <= 0.0f) m_HitTimer = 1.0f;
}

void PlayerController::OnInspectorGUI()
{
	ImGui::DragFloat("Speed", &m_Speed, 0.5f, 0.0f, 200.0f);
	ImGui::DragFloat("Jump Power", &m_JumpPower, 0.1f, 0.0f, 100.0f);
	ImGui::Text("Hit Timer: %.2f", m_HitTimer);
	ImGui::Text("Animation: %s -> %s (%.2f)", m_AnimationName.c_str(), m_NextAnimationName.c_str(), m_Blend);
}
