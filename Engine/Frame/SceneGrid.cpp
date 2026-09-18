#include "main.h"
#include "Renderer.h"
#include "ShaderManager.h"
#include "SceneGrid.h"
#include <vector>

ID3D11Buffer* SceneGrid::m_VertexBuffer = nullptr;
UINT          SceneGrid::m_Capacity = 0;
bool          SceneGrid::m_Enable = true;

void SceneGrid::Uninit()
{
	if (m_VertexBuffer) m_VertexBuffer->Release();
	m_VertexBuffer = nullptr;
	m_Capacity = 0;
}

void SceneGrid::Draw(const Vector3& cameraPosition)
{
	if (!m_Enable) return;

	// カメラの高さに合わせて広さを変える（高いところから見るほど広く）
	float height = fabsf(cameraPosition.y);
	int   extent = 40 + (int)(height * 2.0f);
	if (extent > 150) extent = 150;
	const int segment = 4;		// 線を 4m ごとに区切って、遠くほど薄くする

	float centerX = floorf(cameraPosition.x);
	float centerZ = floorf(cameraPosition.z);
	float fadeDistance = (float)extent;

	std::vector<VERTEX_3D> vertices;
	vertices.reserve((extent * 2 + 1) * 2 * (extent * 2 / segment + 1) * 2);

	auto addVertex = [&](float x, float z, const XMFLOAT4& baseColor)
	{
		float dx = x - cameraPosition.x, dz = z - cameraPosition.z;
		float fade = 1.0f - sqrtf(dx * dx + dz * dz) / fadeDistance;
		if (fade < 0.0f) fade = 0.0f;

		VERTEX_3D v{};
		v.Position = XMFLOAT3(x, 0.0f, z);
		v.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
		v.Diffuse = XMFLOAT4(baseColor.x, baseColor.y, baseColor.z, baseColor.w * fade);
		vertices.push_back(v);
	};

	auto lineColor = [](int index, bool axis) -> XMFLOAT4
	{
		if (axis) return XMFLOAT4(0.9f, 0.9f, 0.9f, 0.0f);	// 後で軸の色に置き換える
		if (index % 10 == 0) return XMFLOAT4(1.0f, 1.0f, 1.0f, 0.35f);
		return XMFLOAT4(1.0f, 1.0f, 1.0f, 0.14f);
	};

	for (int i = -extent; i <= extent; i++)
	{
		// X 方向の線（z が一定）
		{
			float z = centerZ + i;
			int worldIndex = (int)z;
			XMFLOAT4 color = (worldIndex == 0) ? XMFLOAT4(0.3f, 0.5f, 1.0f, 0.8f) : lineColor(abs(worldIndex), false);	// Z=0 の線（X軸）… 青っぽく
			if (worldIndex == 0) color = XMFLOAT4(1.0f, 0.35f, 0.35f, 0.8f);	// X 軸は赤
			for (int s = -extent; s < extent; s += segment)
			{
				addVertex(centerX + s, z, color);
				addVertex(centerX + s + segment, z, color);
			}
		}
		// Z 方向の線（x が一定）
		{
			float x = centerX + i;
			int worldIndex = (int)x;
			XMFLOAT4 color = (worldIndex == 0) ? XMFLOAT4(0.35f, 0.55f, 1.0f, 0.8f) : lineColor(abs(worldIndex), false);	// Z 軸は青
			for (int s = -extent; s < extent; s += segment)
			{
				addVertex(x, centerZ + s, color);
				addVertex(x, centerZ + s + segment, color);
			}
		}
	}

	// 頂点バッファ（足りなくなったら作り直す）
	UINT count = (UINT)vertices.size();
	if (count > m_Capacity)
	{
		Uninit();
		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(VERTEX_3D) * count;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Renderer::GetDevice()->CreateBuffer(&bd, nullptr, &m_VertexBuffer);
		m_Capacity = count;
	}
	if (m_VertexBuffer == nullptr) return;

	D3D11_MAPPED_SUBRESOURCE ms;
	if (FAILED(Renderer::GetDeviceContext()->Map(m_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) return;
	memcpy(ms.pData, vertices.data(), sizeof(VERTEX_3D) * count);
	Renderer::GetDeviceContext()->Unmap(m_VertexBuffer, 0);

	// 描画（奥行きの比較はするが書き込まない＝物に隠れるが、物を隠さない）
	ShaderManager::Load("shader\\unlitTextureVS.cso", "shader\\unlitTexturePS.cso")->Set();
	Renderer::SetWorldMatrix(XMMatrixIdentity());

	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = false;
	Renderer::SetMaterial(material);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	Renderer::SetDepthEnable(false);
	Renderer::GetDeviceContext()->Draw(count, 0);
	Renderer::SetDepthEnable(true);

	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
