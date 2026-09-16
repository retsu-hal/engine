#include "main.h"
#include "Renderer.h"
#include "Cube.h"



void CUBE::Init()
{
	m_Position = { 0.0f, 10.0f, 0.0f };
	m_Scale = { 1.0f, 1.0f, 1.0f };
	m_Velocity = { 0.0f, 0.0f, 0.0f };
	m_RotSpeed = { 0.1f, 0.0f, 0.1f };

	static const XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

	static const CUBE::Vertex3D vertexData[] =
	{
		// Position                      Normal                TexCoord
		// 上面 (Y+)　サイコロ（１）
		{ {-1.0f,  1.0f,  1.0f }, { 0.0f,  1.0f,  0.0f }, color, {  0.25f,  0.0f } },
		{ { 1.0f,  1.0f,  1.0f }, { 0.0f,  1.0f,  0.0f }, color, { 0.5f,  0.0f } },
		{ {-1.0f,  1.0f, -1.0f }, { 0.0f,  1.0f,  0.0f }, color, {  0.25f, 0.333f } },
		{ { 1.0f,  1.0f, -1.0f }, { 0.0f,  1.0f,  0.0f }, color, { 0.5f, 0.333f } },

		// 下面 (Y-)　サイコロ（６）
		{ {1.0f, -1.0f,  1.0f }, { 0.0f, -1.0f,  0.0f }, color, {  0.25f,  0.666f } },
		{ {-1.0f, -1.0f,  1.0f }, { 0.0f, -1.0f,  0.0f }, color, { 0.50f,  0.666f } },
		{ {1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f,  0.0f }, color, {  0.25f, 1.0f } },
		{ {- 1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f,  0.0f }, color, { 0.5f, 1.0f } },

		// 前面 (Z+) サイコロ(２)
		{ {1.0f,  1.0f,  1.0f }, { 0.0f,  0.0f,  1.0f }, color, {  0.0f,  0.333f } },
		{ {-1.0f, 1.0f,  1.0f }, { 0.0f,  0.0f,  1.0f }, color, { 0.25f,  0.333f } },
		{ { 1.0f,  -1.0f,  1.0f }, { 0.0f,  0.0f,  1.0f }, color, {  0.0f, 0.666f } },
		{ { -1.0f, -1.0f,  1.0f }, { 0.0f,  0.0f,  1.0f }, color, { 0.25f, 0.666f } },

		// 後面 (Z-) サイコロ（５）
		{ {-1.0f,  1.0f, -1.0f }, { 0.0f,  0.0f, -1.0f }, color, {  0.75f,  0.333f } },
		{ { 1.0f,  1.0f, -1.0f }, { 0.0f,  0.0f, -1.0f }, color, { 0.75f,  0.666f } },
		{ {-1.0f, -1.0f, -1.0f }, { 0.0f,  0.0f, -1.0f }, color, {  1.0f, 0.333f } },
		{ { 1.0f, -1.0f, -1.0f }, { 0.0f,  0.0f, -1.0f }, color, { 1.0f, 0.666f } },

		// 右面 (X+) サイコロ（３）
		{ { 1.0f,  1.0f, -1.0f }, { 1.0f,  0.0f,  0.0f }, color, {  0.25f,  0.333f } },
		{ { 1.0f,  1.0f,  1.0f }, { 1.0f,  0.0f,  0.0f }, color, { 0.5f,  0.333f } },
		{ { 1.0f, -1.0f, -1.0f }, { 1.0f,  0.0f,  0.0f }, color, {  0.25f, 0.666f } },
		{ { 1.0f, -1.0f,  1.0f }, { 1.0f,  0.0f,  0.0f }, color, { 0.5f, 0.666f } },

		// 左面 (X-)
		{ {-1.0f,  1.0f,  1.0f }, { -1.0f, 0.0f,  0.0f }, color, {  0.5f,  0.333f } },
		{ {-1.0f,  1.0f, -1.0f }, { -1.0f, 0.0f,  0.0f }, color, { 0.75f,  0.333f } },
		{ {-1.0f, -1.0f,  1.0f }, { -1.0f, 0.0f,  0.0f }, color, {  0.5f, 0.666f } },
		{ {-1.0f, -1.0f, -1.0f }, { -1.0f, 0.0f,  0.0f }, color, { 0.75f, 0.666f } },
	};
	Vertex3D vertex[24];

	// vertex配列へコピー
	memcpy(vertex, vertexData, sizeof(vertexData));

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(VERTEX_3D) * 24;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	// インデックスバッファ生成
	WORD index[36];
	for (int i = 0; i < 6; i++)
	{
		int base = i * 4;
		index[i * 6 + 0] = base + 0;
		index[i * 6 + 1] = base + 1;
		index[i * 6 + 2] = base + 2;
		index[i * 6 + 3] = base + 2;
		index[i * 6 + 4] = base + 1;
		index[i * 6 + 5] = base + 3;
	}
	
	{
		D3D11_BUFFER_DESC ibd{};
		ibd.Usage = D3D11_USAGE_DEFAULT;
		ibd.ByteWidth = sizeof(WORD) * 36;
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA isd{};
		isd.pSysMem = index;

		Renderer::GetDevice()->CreateBuffer(&ibd, &isd, &m_indexBuffer);

	}

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_vertexBuffer);
	//シェーダー読み込み
	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\unlitTextureVS.cso");
	Renderer::CreatePixelShader(&m_PixelShader, "shader\\unlitTexturePS.cso");

	//テクスチャ読み込み
	TexMetadata metadata;
	ScratchImage image;
	LoadFromWICFile(L"asset\\texture\\dice.jpg", WIC_FLAGS_NONE, &metadata, image);//テクスチャは変更可
	CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
		image.GetImageCount(), metadata, &m_Texture);
	assert(m_Texture);//読み込み失敗時にダイアログを表示
}

void CUBE::Uninit()
{
	m_vertexBuffer->Release();

	m_VertexLayout->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
	m_indexBuffer->Release();
	m_Texture->Release();
}

void CUBE::Update()
{
	m_Rotation.x += m_RotSpeed.x;
	m_Rotation.y += m_RotSpeed.y;
	m_Rotation.z += m_RotSpeed.z;
}

void CUBE::Draw()
{
	//入力レイアウト設定
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	//シェーダー設定
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

	//マトリックス設定
	XMMATRIX WorldMatrix, ScaleMatrix, RotMatrix, TransMatrix;
	ScaleMatrix = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	RotMatrix = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	TransMatrix = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	WorldMatrix = ScaleMatrix * RotMatrix * TransMatrix;

	Renderer::SetWorldMatrix(WorldMatrix);

	//マテリアル設定
	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = true;			//true:テクスチャを使用する、false:テクスチャを使用しない
	Renderer::SetMaterial(material);


	//テクスチャ設定
	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture);

	//頂点バッファ設定
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
	Renderer::GetDeviceContext()->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R16_UINT, 0);
	//プリミティブトポロジ設定
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//描画
	Renderer::GetDeviceContext()->DrawIndexed(36, 0, 0);
}