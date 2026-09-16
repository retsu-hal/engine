#include "main.h"
#include "JsonUtil.h"
#include "Registry.h"
#include "AssetBrowser.h"
#include "Renderer.h"
#include "Manager.h"
#include "GameObject.h"
#include "Camera.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "BillboardRenderer.h"

void BillboardRenderer::Load(const wchar_t* textureFile)
{
	m_TextureFile = textureFile;

	m_Shader = ShaderManager::Load("shader\\unlitTextureVS.cso", "shader\\unlitTexturePS.cso");
	m_Texture = TextureManager::Load(textureFile);

	//頂点は毎フレーム書き換えるので、初期データなしの動的バッファを作る
	if (m_VertexBuffer == nullptr)
	{
		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(VERTEX_3D) * 4;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Renderer::GetDevice()->CreateBuffer(&bd, nullptr, &m_VertexBuffer);
	}
}

void BillboardRenderer::Uninit()
{
	if (m_VertexBuffer)
	{
		m_VertexBuffer->Release();
		m_VertexBuffer = nullptr;
	}
}

void BillboardRenderer::Draw()
{
	if (m_VertexBuffer == nullptr || m_Texture == nullptr) return;	// テクスチャ未読み込み

	CAMERA* camera = Manager::GetGameObject<CAMERA>();
	if (camera == nullptr) return;

	ID3D11DeviceContext* context = Renderer::GetDeviceContext();

	//頂点更新（サイズ・色・UV）
	float hw = m_Width * 0.5f;
	float top = m_AnchorBottom ? m_Height : m_Height * 0.5f;
	float bottom = m_AnchorBottom ? 0.0f : -m_Height * 0.5f;
	float u0 = m_UV.x, v0 = m_UV.y;
	float u1 = m_UV.x + m_UV.z, v1 = m_UV.y + m_UV.w;
	XMFLOAT3 normal(0.0f, 0.0f, -1.0f);

	D3D11_MAPPED_SUBRESOURCE msr;
	context->Map(m_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;
	vertex[0] = { XMFLOAT3(-hw, top,    0.0f), normal, m_Color, XMFLOAT2(u0, v0) };
	vertex[1] = { XMFLOAT3(hw, top,    0.0f), normal, m_Color, XMFLOAT2(u1, v0) };
	vertex[2] = { XMFLOAT3(-hw, bottom, 0.0f), normal, m_Color, XMFLOAT2(u0, v1) };
	vertex[3] = { XMFLOAT3(hw, bottom, 0.0f), normal, m_Color, XMFLOAT2(u1, v1) };
	context->Unmap(m_VertexBuffer, 0);

	//ビルボード回転
	XMMATRIX billboard;
	if (m_Mode == BillboardMode::Full)
	{
		billboard = XMMatrixInverse(nullptr, camera->GetViewMatrix());
		billboard.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);	//平行移動成分を消す
	}
	else
	{
		Vector3 forward = camera->GetForward();
		billboard = XMMatrixRotationY(atan2f(forward.x, forward.z));
	}

	//ワールド行列
	Vector3 pos = m_GameObject->GetPosition();
	Vector3 scale = m_GameObject->GetScale();
	XMMATRIX world = XMMatrixScaling(scale.x, scale.y, scale.z) * billboard
		* XMMatrixTranslation(pos.x, pos.y, pos.z);
	Renderer::SetWorldMatrix(world);

	//マテリアル（色は頂点カラーで指定済み）
	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = true;
	Renderer::SetMaterial(material);

	//描画
	m_Shader->Set();
	context->PSSetShaderResources(0, 1, &m_Texture);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	if (m_UseATC) Renderer::SetATCEnable(true);
	context->Draw(4, 0);
	if (m_UseATC) Renderer::SetATCEnable(false);
}

void BillboardRenderer::OnInspectorGUI()
{
	ImGui::Text("Texture: %s", m_TextureFile.empty() ? "(なし) ← 画像をドロップ" : WideToUtf8(m_TextureFile).c_str());
	std::string path;
	if (AssetBrowser::AcceptDrop(AssetBrowser::AssetType::Texture, path)) Load(Utf8ToWide(path).c_str());
	int mode = (int)m_Mode;
	if (ImGui::Combo("Mode", &mode, "Full\0AxisY\0")) m_Mode = (BillboardMode)mode;
	ImGui::DragFloat("Width", &m_Width, 0.05f);
	ImGui::DragFloat("Height", &m_Height, 0.05f);
	ImGui::Checkbox("Anchor Bottom", &m_AnchorBottom);
	ImGui::ColorEdit4("Color", &m_Color.x);
}

void BillboardRenderer::Serialize(nlohmann::json& data) const
{
	data["texture"] = WideToUtf8(m_TextureFile);
	data["mode"] = (int)m_Mode;
	data["width"] = m_Width;
	data["height"] = m_Height;
	data["anchorBottom"] = m_AnchorBottom;
	data["atc"] = m_UseATC;
	data["color"] = ToJson(m_Color);
	data["uv"] = ToJson(m_UV);
}

void BillboardRenderer::Deserialize(const nlohmann::json& data)
{
	std::string texture;
	JsonRead(data, "texture", texture);
	if (!texture.empty()) Load(Utf8ToWide(texture).c_str());

	int mode = (int)m_Mode;
	JsonRead(data, "mode", mode);
	m_Mode = (BillboardMode)mode;
	JsonRead(data, "width", m_Width);
	JsonRead(data, "height", m_Height);
	JsonRead(data, "anchorBottom", m_AnchorBottom);
	JsonRead(data, "atc", m_UseATC);
	JsonRead(data, "color", m_Color);
	JsonRead(data, "uv", m_UV);
}

REGISTER_COMPONENT(BillboardRenderer)
