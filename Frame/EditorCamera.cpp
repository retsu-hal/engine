#include "main.h"
#include "Renderer.h"
#include "EditorCamera.h"

Vector3 EditorCamera::m_Position{ 0.0f, 10.0f, -15.0f };
float   EditorCamera::m_Yaw = 0.0f;
float   EditorCamera::m_Pitch = 0.4f;
float   EditorCamera::m_MoveSpeed = 15.0f;
bool    EditorCamera::m_Initialized = false;
bool    EditorCamera::m_Focusing = false;
Vector3 EditorCamera::m_FocusTarget{ 0.0f, 0.0f, 0.0f };
float   EditorCamera::m_FocusDistance = 8.0f;

void EditorCamera::InitFrom(const Vector3& position, const Vector3& target)
{
	m_Position = position;

	Vector3 dir = target - position;
	float horizontal = sqrtf(dir.x * dir.x + dir.z * dir.z);
	m_Yaw = atan2f(dir.x, dir.z);
	m_Pitch = atan2f(-dir.y, horizontal);

	m_Initialized = true;
}

Vector3 EditorCamera::GetForward()
{
	return Vector3(
		sinf(m_Yaw) * cosf(m_Pitch),
		-sinf(m_Pitch),
		cosf(m_Yaw) * cosf(m_Pitch));
}

void EditorCamera::Update(bool sceneViewHovered)
{
	ImGuiIO& io = ImGui::GetIO();
	float dt = io.DeltaTime;

	static bool dragging = false;

	// シーンビューの上で右クリックしたときだけ操作を始める（他のウィンドウの操作と混ざらないように）
	if (sceneViewHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) dragging = true;
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) dragging = false;

	Vector3 forward = GetForward();
	Vector3 right = Vector3::cross(Vector3(0.0f, 1.0f, 0.0f), forward);
	right.normalize();

	// フォーカス中：目標位置へ少しずつ近づく（自分で操作し始めたらやめる）
	if (dragging || (sceneViewHovered && io.MouseWheel != 0.0f)) m_Focusing = false;
	if (m_Focusing)
	{
		Vector3 goal = m_FocusTarget - forward * m_FocusDistance;
		float t = 1.0f - expf(-12.0f * dt);	// フレームレートに関係なく同じ速さで寄る
		m_Position = m_Position + (goal - m_Position) * t;

		if ((goal - m_Position).length() < 0.01f)
		{
			m_Position = goal;
			m_Focusing = false;
		}
	}

	if (dragging)
	{
		// 視点回転
		const float sensitivity = 0.005f;
		m_Yaw += io.MouseDelta.x * sensitivity;
		m_Pitch += io.MouseDelta.y * sensitivity;

		const float limit = XM_PIDIV2 - 0.01f;
		if (m_Pitch > limit)  m_Pitch = limit;
		if (m_Pitch < -limit) m_Pitch = -limit;

		// 移動（Shift で速く）
		float speed = m_MoveSpeed * (io.KeyShift ? 3.0f : 1.0f) * dt;
		if (ImGui::IsKeyDown(ImGuiKey_W)) m_Position += forward * speed;
		if (ImGui::IsKeyDown(ImGuiKey_S)) m_Position -= forward * speed;
		if (ImGui::IsKeyDown(ImGuiKey_D)) m_Position += right * speed;
		if (ImGui::IsKeyDown(ImGuiKey_A)) m_Position -= right * speed;
		if (ImGui::IsKeyDown(ImGuiKey_E)) m_Position.y += speed;
		if (ImGui::IsKeyDown(ImGuiKey_Q)) m_Position.y -= speed;
	}

	// ホイールで前後移動
	if (sceneViewHovered && io.MouseWheel != 0.0f)
	{
		m_Position += forward * (io.MouseWheel * 2.0f);
	}
}

XMMATRIX EditorCamera::GetViewMatrix()
{
	Vector3 forward = GetForward();
	XMVECTOR eye = XMVectorSet(m_Position.x, m_Position.y, m_Position.z, 1.0f);
	XMVECTOR dir = XMVectorSet(forward.x, forward.y, forward.z, 0.0f);
	return XMMatrixLookToLH(eye, dir, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
}

XMMATRIX EditorCamera::GetProjectionMatrix()
{
	return XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f),
		(float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 2000.0f);
}

void EditorCamera::Apply()
{
	Renderer::SetViewMatrix(GetViewMatrix());
	Renderer::SetProjectionMatrix(GetProjectionMatrix());
}

void EditorCamera::OnInspectorGUI()
{
	ImGui::DragFloat3("Position", &m_Position.x, 0.1f);
	ImGui::DragFloat("Move Speed", &m_MoveSpeed, 0.1f, 0.1f, 200.0f);
}

void EditorCamera::Focus(const Vector3& target, float distance)
{
	m_FocusTarget = target;
	m_FocusDistance = distance;
	m_Focusing = true;
}
