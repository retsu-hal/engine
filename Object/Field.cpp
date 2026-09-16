#include "main.h"
#include "Renderer.h"
#include "Field.h"
#include "Component.h"
#include "Audio.h"



void FIELD::Init()
{
	m_Layer = 1;
	m_Position = { 0.0f, 0.0f, 0.0f };
	m_Scale = {20.0f, 1.0f, 20.0f };
	static const XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

	static const FIELD::Vertex3D vertexData[] =
	{
		// Position                      Normal                TexCoord
		// 上面 (Y+)
		{ {-1.0f,  0.0f,  1.0f }, { 0.0f,  1.0f,  0.0f }, color, {  0.0f,  0.0f } },
		{ { 1.0f,  0.0f,  1.0f }, { 0.0f,  1.0f,  0.0f }, color, { 1.0f,  0.0f } },
		{ {-1.0f,  0.0f, -1.0f }, { 0.0f,  1.0f,  0.0f }, color, {  0.0f, 1.0f } },
		{ { 1.0f,  0.0f, -1.0f }, { 0.0f,  1.0f,  0.0f }, color, { 1.0f, 1.0f } },
	};
	Vertex3D vertex[4];

	// vertex配列へコピー
	memcpy(vertex, vertexData, sizeof(vertexData));

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(VERTEX_3D) * 4;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_vertexBuffer);
	//シェーダー読み込み
	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\unlitTextureVS.cso");
	Renderer::CreatePixelShader(&m_PixelShader, "shader\\unlitTexturePS.cso");

	//テクスチャ読み込み
	TexMetadata metadata;
	ScratchImage image;
	LoadFromWICFile(L"asset\\texture\\iaigami.jpg", WIC_FLAGS_NONE, &metadata, image);//テクスチャは変更可
	CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
		image.GetImageCount(), metadata, &m_Texture);
	assert(m_Texture);//読み込み失敗時にダイアログを表示

	Audio* bgm = AddComponent<Audio>(this);
	bgm->Load("asset\\audio\\bgm.wav");
	bgm->Play(true);
}

void FIELD::Uninit()
{
	m_vertexBuffer->Release();

	m_VertexLayout->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
	m_Texture->Release();

	GameObject::Uninit();
}

void FIELD::Update()
{
	GameObject::Update();
}

void FIELD::Draw()
{
	// カリング無効化（両面描画）
	D3D11_RASTERIZER_DESC rasterDesc{};
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;  // ← 裏面もカリングしない
	rasterDesc.FrontCounterClockwise = FALSE;
	rasterDesc.DepthClipEnable = TRUE;

	ID3D11RasterizerState* pRasterState = nullptr;
	Renderer::GetDevice()->CreateRasterizerState(&rasterDesc, &pRasterState);
	Renderer::GetDeviceContext()->RSSetState(pRasterState);


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

	//プリミティブトポロジ設定
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//描画
	Renderer::GetDeviceContext()->Draw(4, 0);
}