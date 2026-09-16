#include "main.h"
#include "Renderer.h"
#include "Manager.h"
#include "Shadow.h"
#include "MeshField.h"

#define SHADOW_OFFSET_Y	(0.05f)		// 影を地面から浮かせる量（Zファイティング回避）


void Shadow::Init()
{
	m_Layer = 2;
	m_Position = { 0.0f, 0.0f, 0.0f };
	m_Scale = { 1.0f, 1.0f, 1.0f };

	// 頂点バッファ生成（毎フレーム地形の高さで作り直すので DYNAMIC）
	{
		for (int z = 0; z <= DIV; z++)
		{
			for (int x = 0; x <= DIV; x++)
			{
				m_Vertex[z][x].Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
				m_Vertex[z][x].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
				m_Vertex[z][x].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
				m_Vertex[z][x].TexCoord = XMFLOAT2((float)x / DIV, (float)z / DIV);
			}
		}

		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(m_Vertex);
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = m_Vertex;

		Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_vertexBuffer);
	}

	// インデックスバッファ生成（DIV×DIVマスを三角形リストで張る）
	{
		unsigned int index[DIV * DIV * 6];

		int i = 0;
		for (int z = 0; z < DIV; z++)
		{
			for (int x = 0; x < DIV; x++)
			{
				unsigned int v0 = z * (DIV + 1) + x;			// 左上
				unsigned int v1 = v0 + 1;					// 右上
				unsigned int v2 = (z + 1) * (DIV + 1) + x;	// 左下
				unsigned int v3 = v2 + 1;					// 右下

				index[i++] = v0;
				index[i++] = v1;
				index[i++] = v2;

				index[i++] = v1;
				index[i++] = v3;
				index[i++] = v2;
			}
		}

		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(index);
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = index;

		Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_indexBuffer);
	}

	// カリング無効化（両面描画）のラスタライザステートを1回だけ生成
	D3D11_RASTERIZER_DESC rasterDesc{};
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;  // ← 裏面もカリングしない
	rasterDesc.FrontCounterClockwise = FALSE;
	rasterDesc.DepthClipEnable = TRUE;
	// 地形に密着させるので、深度を少しだけ手前に寄せてZファイティングを防ぐ
	rasterDesc.DepthBias = -100;
	rasterDesc.SlopeScaledDepthBias = -1.0f;
	rasterDesc.DepthBiasClamp = 0.0f;
	Renderer::GetDevice()->CreateRasterizerState(&rasterDesc, &m_RasterState);

	//シェーダー読み込み
	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\unlitTextureVS.cso");
	Renderer::CreatePixelShader(&m_PixelShader, "shader\\unlitTexturePS.cso");

	//テクスチャ読み込み
	TexMetadata metadata;
	ScratchImage image;
	LoadFromWICFile(L"asset\\texture\\Shadow.png", WIC_FLAGS_NONE, &metadata, image);//テクスチャは変更可
	CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
		image.GetImageCount(), metadata, &m_Texture);
	assert(m_Texture);//読み込み失敗時にダイアログを表示


}

void Shadow::Uninit()
{
	m_vertexBuffer->Release();
	m_indexBuffer->Release();

	if (m_RasterState)	m_RasterState->Release();

	m_VertexLayout->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
	m_Texture->Release();

	GameObject::Uninit();
}

void Shadow::Update()
{
	GameObject::Update();
}

void Shadow::Draw()
{
	// 影メッシュを地形の高さに貼り付ける。
	// SetPosition する側（Player）より確実に後で走るように、Updateではなく描画直前で作る
	{
		MeshField* meshField = Manager::GetGameObject<MeshField>();

		for (int z = 0; z <= DIV; z++)
		{
			for (int x = 0; x <= DIV; x++)
			{
				// ローカル -1〜1 の広がりをワールド座標へ展開する
				Vector3 world;
				world.x = m_Position.x + ((float)x / DIV * 2.0f - 1.0f) * m_Scale.x;
				world.y = m_Position.y;
				world.z = m_Position.z + (1.0f - (float)z / DIV * 2.0f) * m_Scale.z;

				// 頂点ごとに地形の高さを拾うので、坂でも影が地面に埋まらない
				if (meshField)
					world.y = meshField->GetHeight(world) + SHADOW_OFFSET_Y;

				m_Vertex[z][x].Position = XMFLOAT3(world.x, world.y, world.z);
			}
		}

		D3D11_MAPPED_SUBRESOURCE msr{};
		Renderer::GetDeviceContext()->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
		memcpy(msr.pData, m_Vertex, sizeof(m_Vertex));
		Renderer::GetDeviceContext()->Unmap(m_vertexBuffer, 0);
	}

	// カリング無効化（両面描画）に切り替え。元のステートは描画後に戻す
	ID3D11RasterizerState* prevRasterState = nullptr;
	Renderer::GetDeviceContext()->RSGetState(&prevRasterState);	// RSGetStateはAddRefするので後でRelease
	Renderer::GetDeviceContext()->RSSetState(m_RasterState);

	//入力レイアウト設定
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	//シェーダー設定
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

	//マトリックス設定（頂点をワールド座標で作っているので単位行列）
	Renderer::SetWorldMatrix(XMMatrixIdentity());

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

	//インデックスバッファ設定
	Renderer::GetDeviceContext()->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	//プリミティブトポロジ設定
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 深度書き込みだけOFF（深度テストは残すので、手前の山にはちゃんと隠れる）
	Renderer::SetDepthEnable(false);

	//描画
	Renderer::GetDeviceContext()->DrawIndexed(DIV * DIV * 6, 0, 0);

	Renderer::SetDepthEnable(true);

	//ラスタライザステートを元に戻す（以降の描画がCULL_NONEのままになるのを防ぐ）
	Renderer::GetDeviceContext()->RSSetState(prevRasterState);
	if (prevRasterState)	prevRasterState->Release();
}
