/*==============================================================================

[Score.cpp]
														Author :Watanabe Retsu
														Date   :2026/06/30
--------------------------------------------------------------------------------

==============================================================================*/

//==============================================================================
//インクルード
//==============================================================================
#include "main.h"
#include "Renderer.h"
#include "Manager.h"
#include "Score.h"

//==============================================================================
//マクロ宣言
//==============================================================================
#define TEXTURE_NUM_WIDTH 5
#define TEXTURE_NUM_HEIGHT 5
#define SCORE_MAX 999999
#define DIGIT_WIDTH  50.0f   // 1桁の横幅
#define DIGIT_HEIGHT 50.0f   // 1桁の高さ
#define DIGIT_STEP   30.0f   // 次の桁までの送り幅（詰める/空けるを調整）

//==============================================================================
//プロトタイプ宣言
//==============================================================================

//==============================================================================
//グローバル変数
//==============================================================================

//==============================================================================
//初期化処理
//==============================================================================
void Score::Init()
{
	m_Layer = 3;

	VERTEX_3D vertex[4 * DIGIT_NUM];

	for (int i = 0; i < DIGIT_NUM; i++)
	{
		float offsetX = DIGIT_STEP * i;

		vertex[i * 4 + 0].Position = XMFLOAT3(offsetX, 0.0f, 0.0f);
		vertex[i * 4 + 0].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		vertex[i * 4 + 0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[i * 4 + 0].TexCoord = XMFLOAT2(0.0f, 0.0f);

		vertex[i * 4 + 1].Position = XMFLOAT3(offsetX + DIGIT_WIDTH, 0.0f, 0.0f);
		vertex[i * 4 + 1].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		vertex[i * 4 + 1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[i * 4 + 1].TexCoord = XMFLOAT2(1.0f, 0.0f);

		vertex[i * 4 + 2].Position = XMFLOAT3(offsetX, DIGIT_HEIGHT, 0.0f);
		vertex[i * 4 + 2].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		vertex[i * 4 + 2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[i * 4 + 2].TexCoord = XMFLOAT2(0.0f, 1.0f);

		vertex[i * 4 + 3].Position = XMFLOAT3(offsetX + DIGIT_WIDTH, DIGIT_HEIGHT, 0.0f);
		vertex[i * 4 + 3].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		vertex[i * 4 + 3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[i * 4 + 3].TexCoord = XMFLOAT2(1.0f, 1.0f);
	}

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(VERTEX_3D) * 4 * DIGIT_NUM;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_vertexBuffer);

	//シェーダー読み込み
	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\unlitTextureVS.cso");
	Renderer::CreatePixelShader(&m_PixelShader, "shader\\unlitTexturePS.cso");

	//テクスチャ読み込み
	TexMetadata metadata;
	ScratchImage image;
	LoadFromWICFile(L"asset\\texture\\number.png", WIC_FLAGS_NONE, &metadata, image);
	CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
		image.GetImageCount(), metadata, &m_Texture);
	assert(m_Texture);
}

//==============================================================================
//終了処理
//==============================================================================
void Score::Uninit()
{
	m_vertexBuffer->Release();

	m_VertexLayout->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
	m_Texture->Release();
}

//==============================================================================
//更新処理
//==============================================================================
void Score::Update()
{
	float dt=Manager::GetDeltaTime();

	//カウントアップ処理
	if (m_Score < m_TargetScore)
	{
		m_CountTimer += dt;

		int diff = m_TargetScore - m_Score;
		int step = (diff / 10 > 1) ? diff / 10 : 1;

		const float COUNT_INTERVAL = 0.02f; // 1カウントあたりの間隔（秒）
		if (m_CountTimer >= COUNT_INTERVAL)
		{
			m_CountTimer = 0.0f;
			m_Score += step;
			if (m_Score > m_TargetScore) m_Score = m_TargetScore;

			m_PopTimer = 0.15f; // カウントするたびにポップさせる
		}
	}

	if (m_PopTimer > 0.0f)
	{
		m_PopTimer -= dt;
		if (m_PopTimer < 0.0f) m_PopTimer = 0.0f;
	}

	//上限
	if (m_Score > SCORE_MAX) m_Score = SCORE_MAX;
}

//==============================================================================
//描画処理
//==============================================================================
void Score::Draw()
{
	//入力レイアウト設定
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	//シェーダー設定
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

	//マトリックス設定
	Renderer::SetWorldViewProjection2D();

	//ポップ演出のスケール計算
	float popScale = 1.0f;
	if (m_PopTimer > 0.0f)
	{
		float t = m_PopTimer / 0.15f; // 1.0→0.0で減衰
		popScale = 1.0f + 0.2f * t;   // 最大1.2倍まで膨らむ
	}

	XMMATRIX WorldMatrix, ScaleMatrix, RotMatrix, TransMatrix, PopScaleMatrix;
	ScaleMatrix = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	PopScaleMatrix = XMMatrixScaling(popScale, popScale, 1.0f);
	RotMatrix = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	TransMatrix = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	WorldMatrix = ScaleMatrix * PopScaleMatrix * RotMatrix * TransMatrix;

	Renderer::SetWorldMatrix(WorldMatrix);

	//マテリアル設定
	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = true;
	Renderer::SetMaterial(material);

	//テクスチャ設定
	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture);

	//頂点バッファ設定
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);

	//プリミティブトポロジ設定
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	D3D11_MAPPED_SUBRESOURCE msr;
	Renderer::GetDeviceContext()->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;

	float sizeX = 1.0f / TEXTURE_NUM_WIDTH;
	float sizeY = 1.0f / TEXTURE_NUM_HEIGHT;

	// スコアを桁に分解（上位桁から順に取り出す）
	int digit[DIGIT_NUM];
	int score = m_Score;
	for (int i = DIGIT_NUM - 1; i >= 0; i--)
	{
		digit[i] = score % 10;
		score /= 10;
	}

	for (int i = 0; i < DIGIT_NUM; i++)
	{
		float offsetX = DIGIT_STEP * i;
		int num = digit[i];
		float u = sizeX * (num % TEXTURE_NUM_WIDTH);
		float v = sizeY * (num / TEXTURE_NUM_WIDTH);

		// ★Position/Normal/Diffuseも毎回すべて設定する
		vertex[i * 4 + 0].Position = XMFLOAT3(offsetX, 0.0f, 0.0f);
		vertex[i * 4 + 0].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		vertex[i * 4 + 0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[i * 4 + 0].TexCoord = XMFLOAT2(u, v);

		vertex[i * 4 + 1].Position = XMFLOAT3(offsetX + DIGIT_WIDTH, 0.0f, 0.0f);
		vertex[i * 4 + 1].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		vertex[i * 4 + 1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[i * 4 + 1].TexCoord = XMFLOAT2(u + sizeX, v);

		vertex[i * 4 + 2].Position = XMFLOAT3(offsetX, DIGIT_HEIGHT, 0.0f);
		vertex[i * 4 + 2].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		vertex[i * 4 + 2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[i * 4 + 2].TexCoord = XMFLOAT2(u, v + sizeY);

		vertex[i * 4 + 3].Position = XMFLOAT3(offsetX + DIGIT_WIDTH, DIGIT_HEIGHT, 0.0f);
		vertex[i * 4 + 3].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		vertex[i * 4 + 3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[i * 4 + 3].TexCoord = XMFLOAT2(u + sizeX, v + sizeY);
	}

	Renderer::GetDeviceContext()->Unmap(m_vertexBuffer, 0);

	//描画
	for (int i = 0; i < DIGIT_NUM; i++)
	{
		Renderer::GetDeviceContext()->Draw(4, i * 4);
	}
}