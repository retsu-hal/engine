#include "Manager.h"
#include "Camera.h"

#define POS_MAX 100.0f
#define POS_MIN -100.0f
#define FOV_MAX 46.0f
#define FOV_MIN 44.0f

void CAMERA::Init()
{
	m_Layer = 0;
	m_Position = { 0.0f, 10.0f, -10.0f };
	m_Target = { 0.0f, 0.0f, 0.0f };
	m_Angle = { 0.0f, 0.0f, 0.0f };
	m_Fov = 45.0f;
	m_MoveSpeed = 0.5f;
}

void CAMERA::Uninit()
{

}

void CAMERA::Update()
{
	// 追いかける相手：Tag が Player のオブジェクト（なければ名前が Player のもの）
	// ※ エンジン側はゲームのスクリプト（PlayerController）を知らないので、Tag で探す
	GameObject* player = Manager::FindWithTag("Player");
	if (player == nullptr)
	{
		for (GameObject* object : Manager::GetAllGameObjects())
		{
			if (!object->IsDestroyed() && object->IsActiveInHierarchy() && object->GetName() == "Player") { player = object; break; }
		}
	}
	if (player == nullptr) return;
	Vector3 playerPos = player->GetWorldPosition();

	float dt = Manager::GetDeltaTime();
	if(Input::GetKeyPress(VK_RIGHT))
		m_Rotation.y -= 5.0f * dt;
	if (Input::GetKeyPress(VK_LEFT))
		m_Rotation.y += 5.0f * dt;

	//線形保管
	float t = 0.1f;
	m_Target = m_Target * (1.0f - t) + (playerPos + Vector3(0.0f,2.0f,0.0f)) * t;

	//シェイク
	m_Target+= m_Shake * cosf(m_ShakeTime * 100.0f);
	m_ShakeTime += dt;
	m_Shake *= 0.9f;

	m_Position = m_Target + Vector3(-sinf(m_Rotation.y) * 10.0f, 5.0f, -cosf(m_Rotation.y) * 10.0f);
}

void CAMERA::Draw()
{
	m_ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_Fov), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 1.0f, 1000.0f);
	Renderer::SetProjectionMatrix(m_ProjectionMatrix);
    
    XMFLOAT3 up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	m_ViewMatrix= XMMatrixLookAtLH(XMLoadFloat3((XMFLOAT3*)&m_Position), XMLoadFloat3((XMFLOAT3*)&m_Target), XMLoadFloat3(&up));
	
	Renderer::SetViewMatrix(m_ViewMatrix);
}

#include "Registry.h"
REGISTER_GAMEOBJECT(CAMERA)
