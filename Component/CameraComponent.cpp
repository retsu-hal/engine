#include "main.h"
#include "Renderer.h"
#include "Manager.h"
#include "GameObject.h"
#include "CameraComponent.h"
#include "EditorGUI.h"
#include "Gizmo.h"
#include "JsonUtil.h"
#include "Registry.h"

CameraComponent* CameraComponent::GetMain()
{
	CameraComponent* best = nullptr;
	for (CameraComponent* camera : Manager::FindComponents<CameraComponent>())
	{
		if (!camera->IsEnabled()) continue;
		if (best == nullptr || camera->m_Priority > best->m_Priority) best = camera;
	}
	return best;
}

XMMATRIX CameraComponent::GetViewMatrix() const
{
	// ワールド行列の逆行列がビュー行列（拡縮は外して向きと位置だけ使う）
	XMMATRIX world = m_GameObject->GetWorldMatrix();
	XMVECTOR scale, rotation, translation;
	XMMatrixDecompose(&scale, &rotation, &translation, world);
	XMMATRIX rigid = XMMatrixRotationQuaternion(rotation) * XMMatrixTranslationFromVector(translation);
	return XMMatrixInverse(nullptr, rigid);
}

XMMATRIX CameraComponent::GetProjectionMatrix() const
{
	return XMMatrixPerspectiveFovLH(XMConvertToRadians(m_Fov), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, m_Near, m_Far);
}

void CameraComponent::Apply() const
{
	Renderer::SetViewMatrix(GetViewMatrix());
	Renderer::SetProjectionMatrix(GetProjectionMatrix());
}

void CameraComponent::Draw()
{
	if (!EditorGUI::UseEditorCamera()) return;	// Play 中は枠を出さない

	// 視野の四角すい（遠くまで描くと見づらいので 3m 分だけ）
	XMMATRIX inverse = XMMatrixInverse(nullptr, GetViewMatrix());
	float length = 3.0f;
	float h = tanf(XMConvertToRadians(m_Fov) * 0.5f) * length;
	float w = h * (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;

	auto toWorld = [&](float x, float y, float z)
	{
		Vector3 p;
		XMStoreFloat3((XMFLOAT3*)&p, XMVector3TransformCoord(XMVectorSet(x, y, z, 1.0f), inverse));
		return p;
	};

	Vector3 eye = toWorld(0, 0, 0);
	Vector3 c[4] = { toWorld(-w, h, length), toWorld(w, h, length), toWorld(w, -h, length), toWorld(-w, -h, length) };
	const XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	for (int i = 0; i < 4; i++)
	{
		Gizmo::DrawLine(eye, c[i], color);
		Gizmo::DrawLine(c[i], c[(i + 1) % 4], color);
	}
}

void CameraComponent::OnInspectorGUI()
{
	ImGui::DragFloat("FOV", &m_Fov, 0.5f, 1.0f, 179.0f);
	ImGui::DragFloat("Near", &m_Near, 0.01f, 0.01f, 10.0f);
	ImGui::DragFloat("Far", &m_Far, 1.0f, 1.0f, 100000.0f);
	ImGui::DragInt("Priority", &m_Priority);
	if (GetMain() == this) ImGui::TextDisabled("このカメラがゲーム画面に使われています");
}

void CameraComponent::Serialize(nlohmann::json& data) const
{
	data["fov"] = m_Fov;
	data["near"] = m_Near;
	data["far"] = m_Far;
	data["priority"] = m_Priority;
}

void CameraComponent::Deserialize(const nlohmann::json& data)
{
	JsonRead(data, "fov", m_Fov);
	JsonRead(data, "near", m_Near);
	JsonRead(data, "far", m_Far);
	JsonRead(data, "priority", m_Priority);
}

REGISTER_COMPONENT(CameraComponent)
