/*==============================================================================

	[Particle.cpp]
														Author :Watanabe Retsu
														Date   :2026/06/18
--------------------------------------------------------------------------------

==============================================================================*/

//==============================================================================
//インクルード
//==============================================================================
#include "main.h"
#include "Manager.h"
#include "Renderer.h"
#include "Particle.h"
#include "Camera.h"

//==============================================================================
//マクロ宣言
//==============================================================================

//==============================================================================
//プロトタイプ宣言
//==============================================================================

//==============================================================================
//グローバル変数
//==============================================================================

//==============================================================================
//初期化処理
//==============================================================================
void Particle::Init()
{
	m_Layer = 2;


	VERTEX_3D vertex[4];

	{
		vertex[0].Position = XMFLOAT3(-0.5f, 0.5f, 0.0f);
		vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

		vertex[1].Position = XMFLOAT3(0.5f, 0.5f, 0.0f);
		vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

		vertex[2].Position = XMFLOAT3(-0.5f, -0.5f, 0.0f);
		vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

		vertex[3].Position = XMFLOAT3(0.5f, -0.5f, 0.0f);
		vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);
	}

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
	LoadFromWICFile(L"asset\\texture\\particle.png", WIC_FLAGS_NONE, &metadata, image);//テクスチャは変更可
	CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
		image.GetImageCount(), metadata, &m_Texture);
	assert(m_Texture);//読み込み失敗時にダイアログを表示

	//パーティクルの初期化
	for(int i = 0; i < PARTICLE_MAX; i++)
	{
		m_Particle[i].Enable = false;
	}
}

//==============================================================================
//終了処理
//==============================================================================
void Particle::Uninit()
{
	m_vertexBuffer->Release();

	m_VertexLayout->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
	m_Texture->Release();

	GameObject::Uninit();
}

//==============================================================================
//更新処理
//==============================================================================
void Particle::Update()
{
	float dt = Manager::GetDeltaTime();
	int count = 10;

	//パーティクル発射

		for (int i = 0; i < PARTICLE_MAX; i++)
		{
			if (!m_Particle[i].Enable)
			{
				m_Particle[i].Enable = true;
				m_Particle[i].Life = 60;
				m_Particle[i].Position = m_Position;
				m_Particle[i].Velocity.x = ((float)rand() / RAND_MAX - 0.5f) * 30.0f;
				m_Particle[i].Velocity.y = ((float)rand() / RAND_MAX - 0.5f) * 30.0f;
				m_Particle[i].Velocity.z = ((float)rand() / RAND_MAX - 0.5f) * 30.0f;

				count--;
				if (count <= 0)
				break;
			}
		}


	Vector3 gravity{ 0.0f, -9.8f, 0.0f }; // 重力加速度

	//パーティクルの更新
	for (int i = 0; i < PARTICLE_MAX; i++)
	{
		if (m_Particle[i].Enable)
		{
			m_Particle[i].Velocity += gravity * dt; 
			m_Particle[i].Velocity += m_Particle[i].Velocity * -1.0f * dt;  
			m_Particle[i].Position += m_Particle[i].Velocity * dt;

			m_Particle[i].Life--;
			if (m_Particle[i].Life <= 0)	m_Particle[i].Enable = false;

		}
	}

}

//==============================================================================
//描画処理
//==============================================================================
void Particle::Draw()
{
	//入力レイアウト設定
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	//シェーダー設定
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

	//ビルボード用マトリクス（ビュー行列の逆行列で回転だけ打ち消す）
	CAMERA* camera = Manager::GetGameObject<CAMERA>();
	XMMATRIX ViewMatrix = camera->GetViewMatrix();
	XMMATRIX invViewMatrix = XMMatrixInverse(NULL, ViewMatrix);
	invViewMatrix.r[3].m128_f32[0] = 0.0f;
	invViewMatrix.r[3].m128_f32[1] = 0.0f;
	invViewMatrix.r[3].m128_f32[2] = 0.0f;


	//マテリアル設定
	MATERIAL material{};
	material.Diffuse = XMFLOAT4(0.2f, 0.2f, 1.0f, 1.0f);
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
	
	//深度バッファを無効
	Renderer::SetDepthEnable(false);

	//加算合成を有効
	Renderer::SetAddEnable(true);
	
	for (int i = 0; i < PARTICLE_MAX; i++)
	{
		if (m_Particle[i].Enable)
		{
			//マトリックス設定
			XMMATRIX WorldMatrix, ScaleMatrix, TransMatrix;
			ScaleMatrix = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);					//拡大縮小
			TransMatrix = XMMatrixTranslation(m_Particle[i].Position.x, m_Particle[i].Position.y, m_Particle[i].Position.z);		//平行移動
			WorldMatrix = ScaleMatrix * invViewMatrix * TransMatrix;

			Renderer::SetWorldMatrix(WorldMatrix);

			//描画
			Renderer::GetDeviceContext()->Draw(4, 0);
		}
	}

	
	Renderer::SetDepthEnable(true);	//深度バッファを有効
	Renderer::SetAddEnable(false);		//加算合成を無効
}
