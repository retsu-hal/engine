/*==============================================================================

[MeshField.cpp]
														Author :Watanabe Retsu
														Date   :2026/09/09
--------------------------------------------------------------------------------

==============================================================================*/

//==============================================================================
//インクルード
//==============================================================================
#include"main.h"
#include"renderer.h"
#include"meshField.h"
#include"audio.h"

//==============================================================================
//マクロ宣言
//==============================================================================

//==============================================================================
//プロトタイプ宣言
//==============================================================================

//==============================================================================
//グローバル変数
//==============================================================================
float g_FieldHeight[21][21] =
{
    {5.0f, 6.3f, 7.2f, 7.5f, 7.2f, 6.3f, 4.9f, 3.4f, 3.1f, 3.9f, 4.3f, 4.2f, 3.6f, 2.7f, 2.4f, 2.8f, 2.9f, 2.5f, 1.9f, 1.2f, 0.5f},
    {5.6f, 7.1f, 8.0f, 8.4f, 8.0f, 7.0f, 5.6f, 4.0f, 3.4f, 4.2f, 4.6f, 4.5f, 3.8f, 2.8f, 3.5f, 4.1f, 4.2f, 3.8f, 3.1f, 2.1f, 1.2f},
    {5.9f, 7.3f, 8.2f, 8.6f, 8.2f, 7.3f, 5.8f, 4.2f, 3.3f, 4.0f, 4.4f, 4.2f, 3.5f, 3.4f, 4.5f, 5.2f, 5.3f, 5.0f, 4.1f, 3.0f, 1.9f},
    {5.6f, 6.9f, 7.8f, 8.1f, 7.8f, 6.9f, 5.6f, 4.1f, 2.7f, 3.4f, 3.7f, 3.5f, 2.8f, 3.9f, 5.0f, 5.8f, 6.1f, 5.8f, 4.9f, 3.7f, 2.4f},
    {5.0f, 6.1f, 6.8f, 7.1f, 6.8f, 6.1f, 5.0f, 3.6f, 2.1f, 1.9f, 2.0f, 1.9f, 2.3f, 3.9f, 5.1f, 6.0f, 6.3f, 6.0f, 5.3f, 4.1f, 2.8f},
    {4.0f, 4.9f, 5.4f, 5.7f, 5.5f, 4.9f, 3.6f, 2.0f, 0.9f, 0.5f, 0.4f, 0.4f, 1.1f, 2.4f, 4.3f, 5.6f, 5.9f, 5.7f, 5.1f, 4.0f, 2.7f},
    {2.8f, 3.4f, 3.9f, 4.0f, 3.9f, 3.1f, 1.7f, 0.6f, 0.1f, 0.0f, 0.0f, 0.0f, 0.1f, 0.8f, 2.3f, 4.2f, 5.0f, 4.9f, 4.4f, 3.5f, 2.4f},
    {1.7f, 2.1f, 2.4f, 2.5f, 2.3f, 1.4f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.7f, 2.3f, 3.7f, 3.8f, 3.4f, 2.7f, 2.0f},
    {0.9f, 1.0f, 1.2f, 1.2f, 1.0f, 0.5f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 1.0f, 2.2f, 2.5f, 2.7f, 3.1f, 2.9f},
    {0.4f, 0.4f, 0.4f, 0.4f, 0.3f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.4f, 1.2f, 2.6f, 3.4f, 3.8f, 3.5f},
    {0.3f, 0.3f, 0.3f, 0.3f, 0.3f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 1.3f, 2.8f, 3.6f, 3.9f, 3.6f},
    {0.5f, 0.7f, 1.0f, 1.1f, 0.8f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 1.4f, 2.6f, 3.3f, 3.5f, 3.2f},
    {1.0f, 1.6f, 2.0f, 2.2f, 1.7f, 0.7f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 0.5f, 1.2f, 2.0f, 2.5f, 2.6f, 2.4f},
    {1.7f, 2.6f, 3.2f, 3.5f, 3.1f, 1.7f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 1.6f, 2.1f, 1.7f, 1.5f, 1.6f, 1.4f},
    {2.4f, 3.5f, 4.3f, 4.6f, 4.3f, 3.1f, 1.4f, 0.3f, 0.1f, 0.0f, 0.0f, 0.0f, 0.1f, 0.8f, 2.1f, 3.4f, 3.5f, 2.8f, 1.9f, 1.0f, 0.5f},
    {3.0f, 4.2f, 5.1f, 5.4f, 5.0f, 4.1f, 2.6f, 1.1f, 0.3f, 0.2f, 0.2f, 0.6f, 1.4f, 2.8f, 4.5f, 5.2f, 4.9f, 4.1f, 3.0f, 1.8f, 0.9f},
    {3.2f, 4.4f, 5.3f, 5.6f, 5.2f, 4.4f, 3.1f, 1.8f, 0.7f, 0.4f, 1.1f, 2.1f, 3.6f, 5.2f, 6.1f, 6.4f, 6.0f, 5.2f, 4.0f, 2.6f, 1.4f},
    {3.0f, 4.1f, 4.9f, 5.1f, 4.8f, 4.1f, 2.9f, 1.8f, 0.8f, 0.9f, 1.9f, 3.3f, 4.7f, 6.0f, 6.8f, 7.1f, 6.8f, 5.9f, 4.7f, 3.2f, 1.9f},
    {2.4f, 3.3f, 4.0f, 4.2f, 4.0f, 3.3f, 2.4f, 1.5f, 0.8f, 1.1f, 2.2f, 3.5f, 4.9f, 6.1f, 7.0f, 7.3f, 7.0f, 6.2f, 5.0f, 3.6f, 2.2f},
    {1.7f, 2.3f, 2.8f, 2.9f, 2.8f, 2.3f, 1.7f, 1.1f, 0.7f, 1.2f, 2.1f, 3.4f, 4.6f, 5.8f, 6.5f, 6.8f, 6.6f, 5.9f, 4.8f, 3.5f, 2.2f},
    {1.0f, 1.3f, 1.5f, 1.6f, 1.6f, 1.3f, 1.0f, 0.8f, 0.7f, 1.1f, 1.9f, 2.9f, 4.0f, 4.9f, 5.6f, 5.9f, 5.7f, 5.1f, 4.2f, 3.1f, 2.0f}
};
//==============================================================================
//初期化処理
//==============================================================================
void MeshField::Init()
{
    m_Layer = 1;

    // 頂点バッファ生成
    {
        

        for (int x = 0; x < 21; x++)
        {
            for (int z = 0; z < 21; z++)
            {
                m_Vertex[x][z].Position = XMFLOAT3((x - 10) * 5.0f, g_FieldHeight[z][x], (z - 10) * -5.0f);
                m_Vertex[x][z].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
                m_Vertex[x][z].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
                m_Vertex[x][z].TexCoord = XMFLOAT2((float)x, (float)z);
            }
        }

        for (int x = 1; x < 20; x++)
        {
            for (int z = 1; z < 20; z++)
            {
                Vector3 vx, vz, vn;

                vx.x = m_Vertex[x + 1][z].Position.x - m_Vertex[x - 1][z].Position.x;
                vx.y = m_Vertex[x + 1][z].Position.y - m_Vertex[x - 1][z].Position.y;
                vx.z = m_Vertex[x + 1][z].Position.z - m_Vertex[x - 1][z].Position.z;

                vz.x = m_Vertex[x][z - 1].Position.x - m_Vertex[x][z + 1].Position.x;
                vz.y = m_Vertex[x][z - 1].Position.y - m_Vertex[x][z + 1].Position.y;
                vz.z = m_Vertex[x][z - 1].Position.z - m_Vertex[x][z + 1].Position.z;

                vn = Vector3::cross(vz, vx); // 外積
                vn.normalize(); // 正規化

                m_Vertex[x][z].Normal.x = vn.x;
                m_Vertex[x][z].Normal.y = vn.y;
                m_Vertex[x][z].Normal.z = vn.z;
            }
        }

        // 頂点バッファ生成
        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(VERTEX_3D) * 21 * 21;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = m_Vertex;

        Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_vertexBuffer);
    }

    // インデックスバッファ生成
    {
        unsigned int index[(22 * 2) * 20 - 2];

        int i = 0;

        for (int x = 0; x < 20; x++)
        {
            for (int z = 0; z < 21; z++)
            {
                index[i] = x * 21 + z;
                i++;

                index[i] = (x + 1) * 21 + z;
                i++;
            }

            if (x == 19)
                break;

            // 縮退ポリゴン
            index[i] = (x + 1) * 21 + 20;
            i++;

            index[i] = (x + 1) * 21;
            i++;
        }

        // 頂点バッファ生成
        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(unsigned int) * ((22 * 2) * 20 - 2);
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bd.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = index;

        Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_indexBuffer);
    }

    // シェーダー読込
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\fieldVS.cso");
    Renderer::CreatePixelShader(&m_PixelShader, "shader\\fieldPS.cso");

    // テクスチャ読み込み
    TexMetadata metadata;
    ScratchImage image;
    LoadFromWICFile(L"asset\\texture\\grass.jpg", WIC_FLAGS_NONE, &metadata, image);
    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(), image.GetImageCount(), metadata, &m_Texture);
    assert(m_Texture);

    Audio* bgm = AddComponent<Audio>(this);
    bgm->Load("asset\\audio\\bgm.wav");
    //bgm->Play(true);
}

//==============================================================================
//終了処理
//==============================================================================
void MeshField::Uninit()
{
    m_Texture->Release();

    m_vertexBuffer->Release();
    m_indexBuffer->Release();

    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();

    GameObject::Uninit();
}

//==============================================================================
//更新処理
//==============================================================================
void MeshField::Update()
{
    GameObject::Update();
}

//==============================================================================
//描画処理
//==============================================================================
void MeshField::Draw()
{
    // 入力レイアウト設定
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

    // シェーダ設定
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    // マトリクス設定
    XMMATRIX world, scale, rot, trans;
    scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
    rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
    trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    world = scale * rot * trans;

    Renderer::SetWorldMatrix(world);

    // マテリアル設定
    MATERIAL material{};
    material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
    material.TextureEnable = true;
    Renderer::SetMaterial(material);

    // テクスチャ設定
    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture);

    // 頂点バッファ設定
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);

    // インデックスバッファ設定
    Renderer::GetDeviceContext()->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);

    // プリミティブトポロジ設定
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // ポリゴン描画
    // Renderer::GetDeviceContext()->Draw(21 * 21, 0);
    Renderer::GetDeviceContext()->DrawIndexed((22 * 2) * 20 - 2, 0, 0);
}

float MeshField::GetHeight(Vector3 position)
{
    int x, z;
    
    //ブロック番号算出
    // 頂点間隔は5.0f、かつZは (z - 10) * -5.0f と反転して生成している
    x = (int)(position.x / 5.0f + 10.0f);
    z = (int)(10.0f - position.z / 5.0f);

    // フィールド外は端のマスにクランプ（m_Vertex[x + 1][z + 1] の範囲外アクセス防止）
    if (x < 0)  x = 0;
    if (x > 19) x = 19;
    if (z < 0)  z = 0;
    if (z > 19) z = 19;

	XMFLOAT3 pos0, pos1, pos2, pos3;

	pos0 = m_Vertex[x][z].Position;                         //左上
	pos1 = m_Vertex[x + 1][z].Position;                 //右上
	pos2 = m_Vertex[x][z + 1].Position;                 //左下
	pos3 = m_Vertex[x + 1][z + 1].Position;         //右下

    Vector3 v12, v1p;
	v12.x = pos2.x - pos1.x;
    v12.y = pos2.y - pos1.y;
	v12.z = pos2.z - pos1.z;
    
    v1p.x = position.x - pos1.x;
    v1p.y = position.y - pos1.y;
    v1p.z = position.z - pos1.z;

    //外積
	float cy = v12.z * v1p.x - v12.x * v1p.z;

    float py;
    Vector3 n;
    if (cy > 0.0f)
    {
        //左上
        Vector3 v10;
		v10.x = pos0.x - pos1.x;
		v10.y = pos0.y - pos1.y;
		v10.z = pos0.z - pos1.z;
		
        //外積
		n = Vector3::cross(v12, v10);
	}
    else
    {
        //右下
        Vector3 v13;
        v13.x = pos3.x - pos1.x;
        v13.y = pos3.y - pos1.y;
        v13.z = pos3.z - pos1.z;

        //外積
        n = Vector3::cross(v13, v12);
    }

    //高さ取得
    py = -((position.x - pos1.x) * n.x + (position.z - pos1.z) * n.z) / n.y + pos1.y;

    return py;
}
