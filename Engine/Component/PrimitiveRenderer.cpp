#include "main.h"
#include "Renderer.h"
#include "GameObject.h"
#include "ShaderManager.h"
#include "PrimitiveRenderer.h"
#include "JsonUtil.h"
#include "Registry.h"
#include <vector>

PrimitiveRenderer::Mesh PrimitiveRenderer::m_Meshes[(int)PrimitiveRenderer::Shape::Count];

//=============================================================
// メッシュ作成
//=============================================================
namespace
{
	struct MeshBuilder
	{
		std::vector<VERTEX_3D>    Vertices;
		std::vector<unsigned int> Indices;

		unsigned int AddVertex(const XMFLOAT3& position, const XMFLOAT3& normal, float u, float v)
		{
			VERTEX_3D vertex{};
			vertex.Position = position;
			vertex.Normal = normal;
			vertex.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			vertex.TexCoord = XMFLOAT2(u, v);
			Vertices.push_back(vertex);
			return (unsigned int)Vertices.size() - 1;
		}

		// 三角形を足す。外側から見て時計回り（DirectX の表面）になるよう、法線を見て向きを自動でそろえる
		void AddTriangle(unsigned int a, unsigned int b, unsigned int c)
		{
			XMVECTOR pa = XMLoadFloat3(&Vertices[a].Position);
			XMVECTOR pb = XMLoadFloat3(&Vertices[b].Position);
			XMVECTOR pc = XMLoadFloat3(&Vertices[c].Position);
			XMVECTOR faceNormal = XMVector3Cross(pb - pa, pc - pa);

			XMVECTOR normal = XMLoadFloat3(&Vertices[a].Normal) + XMLoadFloat3(&Vertices[b].Normal) + XMLoadFloat3(&Vertices[c].Normal);

			if (XMVectorGetX(XMVector3Dot(faceNormal, normal)) < 0.0f) { unsigned int t = b; b = c; c = t; }

			// 極の部分などでつぶれた三角形はとばす
			if (XMVectorGetX(XMVector3LengthSq(faceNormal)) < 1e-12f) return;

			Indices.push_back(a);
			Indices.push_back(b);
			Indices.push_back(c);
		}

		void AddQuad(unsigned int a, unsigned int b, unsigned int c, unsigned int d)
		{
			AddTriangle(a, b, c);
			AddTriangle(a, c, d);
		}
	};

	void BuildCube(MeshBuilder& builder)
	{
		// 6面それぞれに4頂点（面ごとに法線を分けて角をくっきりさせる）
		const XMFLOAT3 normals[6] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
		for (const XMFLOAT3& n : normals)
		{
			XMVECTOR normal = XMLoadFloat3(&n);
			XMVECTOR up = (fabsf(n.y) > 0.5f) ? XMVectorSet(0, 0, 1, 0) : XMVectorSet(0, 1, 0, 0);
			XMVECTOR side = XMVector3Cross(up, normal);

			unsigned int index[4];
			const float s[4][2] = { {-1,-1}, {1,-1}, {1,1}, {-1,1} };
			for (int i = 0; i < 4; i++)
			{
				XMFLOAT3 p;
				XMStoreFloat3(&p, (normal + side * s[i][0] + up * s[i][1]) * 0.5f);
				index[i] = builder.AddVertex(p, n, (s[i][0] + 1) * 0.5f, 1.0f - (s[i][1] + 1) * 0.5f);
			}
			builder.AddQuad(index[0], index[1], index[2], index[3]);
		}
	}

	// 球（halfHeight > 0 で上下に離すとカプセルになる）
	void BuildSphere(MeshBuilder& builder, float radius, float halfHeight)
	{
		const int slices = 24;			// 横の分割
		const int stacksPerHalf = 8;	// 半球あたりの縦の分割

		// 上半球（北極→赤道）と下半球（赤道→南極）を別の行として作る
		// カプセルのときは赤道の行が2本になり、その間が円柱になる
		std::vector<std::vector<unsigned int>> rows;
		for (int half = 0; half < 2; half++)
		{
			float offsetY = (half == 0) ? halfHeight : -halfHeight;
			for (int stack = 0; stack <= stacksPerHalf; stack++)
			{
				float latitude = (half == 0)
					? XM_PIDIV2 - XM_PIDIV2 * stack / stacksPerHalf		// 90度 → 0度
					: -XM_PIDIV2 * stack / stacksPerHalf;				// 0度 → -90度

				if (halfHeight <= 0.0f && half == 1 && stack == 0) continue;	// 球は赤道を重複させない

				std::vector<unsigned int> row;
				for (int slice = 0; slice <= slices; slice++)
				{
					float longitude = XM_2PI * slice / slices;
					XMFLOAT3 n(cosf(latitude) * cosf(longitude), sinf(latitude), cosf(latitude) * sinf(longitude));
					XMFLOAT3 p(n.x * radius, n.y * radius + offsetY, n.z * radius);
					row.push_back(builder.AddVertex(p, n, (float)slice / slices, 0.5f - p.y / (2.0f * (radius + halfHeight))));
				}
				rows.push_back(row);
			}
		}

		for (size_t r = 0; r + 1 < rows.size(); r++)
		{
			for (int slice = 0; slice < slices; slice++)
			{
				builder.AddQuad(rows[r][slice], rows[r][slice + 1], rows[r + 1][slice + 1], rows[r + 1][slice]);
			}
		}
	}
}

void PrimitiveRenderer::CreateMesh(Shape shape)
{
	Mesh& mesh = m_Meshes[(int)shape];
	if (mesh.VertexBuffer) return;

	MeshBuilder builder;
	switch (shape)
	{
	case Shape::Cube:    BuildCube(builder); break;
	case Shape::Sphere:  BuildSphere(builder, 0.5f, 0.0f); break;
	case Shape::Capsule: BuildSphere(builder, 0.5f, 0.5f); break;
	default: return;
	}

	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(VERTEX_3D) * (UINT)builder.Vertices.size();
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = builder.Vertices.data();
	Renderer::GetDevice()->CreateBuffer(&bd, &sd, &mesh.VertexBuffer);

	bd.ByteWidth = sizeof(unsigned int) * (UINT)builder.Indices.size();
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	sd.pSysMem = builder.Indices.data();
	Renderer::GetDevice()->CreateBuffer(&bd, &sd, &mesh.IndexBuffer);

	mesh.IndexCount = (UINT)builder.Indices.size();
}

void PrimitiveRenderer::UnloadAll()
{
	for (Mesh& mesh : m_Meshes)
	{
		if (mesh.VertexBuffer) mesh.VertexBuffer->Release();
		if (mesh.IndexBuffer)  mesh.IndexBuffer->Release();
		mesh = Mesh();
	}
}

//=============================================================
// 描画
//=============================================================
void PrimitiveRenderer::Init()
{
	// 形が分かるように、光の当たり方を計算するシェーダーを使う
	m_Shader = ShaderManager::Load("shader\\litVS.cso", "shader\\litPS.cso");
}

void PrimitiveRenderer::Draw()
{
	CreateMesh(m_Shape);
	Mesh& mesh = m_Meshes[(int)m_Shape];
	if (mesh.VertexBuffer == nullptr || m_Shader == nullptr) return;

	m_Shader->Set();
	Renderer::SetWorldMatrix(m_GameObject->GetWorldMatrix());

	MATERIAL material{};
	material.Diffuse = m_Color;
	material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = false;
	Renderer::SetMaterial(material);

	// 真上からの光だと側面が真っ暗になるので、描く間だけ斜めの光にする（終わったら元の光に戻す）
	LIGHT light{};
	light.Enable = true;
	light.Direction = XMFLOAT4(-0.4f, -1.0f, 0.6f, 0.0f);
	light.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	light.Ambient = XMFLOAT4(0.35f, 0.35f, 0.35f, 1.0f);
	Renderer::SetLight(light);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &mesh.VertexBuffer, &stride, &offset);
	Renderer::GetDeviceContext()->IASetIndexBuffer(mesh.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Renderer::GetDeviceContext()->DrawIndexed(mesh.IndexCount, 0, 0);

	// Renderer::Init と同じ光に戻す
	light.Direction = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	light.Ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	light.Diffuse = XMFLOAT4(1.5f, 1.5f, 1.5f, 1.0f);
	Renderer::SetLight(light);
}

//=============================================================
// Inspector・保存
//=============================================================
void PrimitiveRenderer::OnInspectorGUI()
{
	int shape = (int)m_Shape;
	if (ImGui::Combo("Shape", &shape, "Cube\0Sphere\0Capsule\0")) m_Shape = (Shape)shape;
	ImGui::ColorEdit4("Color", &m_Color.x);
}

void PrimitiveRenderer::Serialize(nlohmann::json& data) const
{
	data["shape"] = (int)m_Shape;
	data["color"] = ToJson(m_Color);
}

void PrimitiveRenderer::Deserialize(const nlohmann::json& data)
{
	int shape = (int)m_Shape;
	JsonRead(data, "shape", shape);
	if (shape >= 0 && shape < (int)Shape::Count) m_Shape = (Shape)shape;
	JsonRead(data, "color", m_Color);
}

REGISTER_COMPONENT(PrimitiveRenderer)
