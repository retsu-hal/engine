#include "main.h"
#include "Renderer.h"
#include "AnimationModel.h"
#include "ShaderManager.h"
#include "GameObject.h"

void AnimationModel::Draw()
{
	m_Shader->Set();
	Renderer::SetWorldMatrix(m_GameObject->GetWorldMatrix());

	// プリミティブトポロジ設定
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// マテリアル設定
	MATERIAL material;
	ZeroMemory(&material, sizeof(material));
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = true;
	Renderer::SetMaterial(material);

	for (unsigned int m = 0; m < m_AiScene->mNumMeshes; m++)
	{
		aiMesh* mesh = m_AiScene->mMeshes[m];


		// マテリアル設定
		aiString texture;
		aiColor3D diffuse;
		float opacity;

		aiMaterial* aimaterial = m_AiScene->mMaterials[mesh->mMaterialIndex];
		aimaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texture);
		aimaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
		aimaterial->Get(AI_MATKEY_OPACITY, opacity);

		if (texture == aiString(""))
		{
			material.TextureEnable = false;
		}
		else
		{
			Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture[texture.data]);
			material.TextureEnable = true;
		}

		material.Diffuse = XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, opacity);
		material.Ambient = material.Diffuse;
		Renderer::SetMaterial(material);
		
		Renderer::GetDeviceContext()->VSSetConstantBuffers(6,1,&m_BoneBuffer);

		// 頂点バッファ設定
		UINT stride = sizeof(VERTEX_SKIN);
		UINT offset = 0;
		Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer[m], &stride, &offset);

		// インデックスバッファ設定
		Renderer::GetDeviceContext()->IASetIndexBuffer(m_IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

		// ポリゴン描画
		Renderer::GetDeviceContext()->DrawIndexed(mesh->mNumFaces * 3, 0, 0);
	}
}

void AnimationModel::SetShader(const char* vsFile, const char* psFile)
{
	m_Shader = ShaderManager::Load(vsFile, psFile);
}

void AnimationModel::Load(const char* FileName)
{
	if (m_Shader == nullptr)
		m_Shader = ShaderManager::Load("shader\\SkinVS.cso", "shader\\unlitTexturePS.cso");

	const std::string modelPath(FileName);

	m_AiScene = aiImportFile(FileName, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded);
	assert(m_AiScene);

	m_VertexBuffer = new ID3D11Buffer * [m_AiScene->mNumMeshes];
	m_IndexBuffer = new ID3D11Buffer * [m_AiScene->mNumMeshes];


	//変形後頂点配列生成
	m_DeformVertex = new std::vector<DEFORM_VERTEX>[m_AiScene->mNumMeshes];

	//再帰的にボーン生成
	CreateBone(m_AiScene->mRootNode);

	// ボーン行列用定数バッファ（1回だけ）
	{
		D3D11_BUFFER_DESC cbd{};
		cbd.ByteWidth = sizeof(XMFLOAT4X4) * MAX_BONE;
		cbd.Usage = D3D11_USAGE_DYNAMIC;
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Renderer::GetDevice()->CreateBuffer(&cbd, nullptr, &m_BoneBuffer);
	}

	for (unsigned int m = 0; m < m_AiScene->mNumMeshes; m++)
	{
		aiMesh* mesh = m_AiScene->mMeshes[m];

		//変形後頂点データ初期化（← 先にやる）
		m_DeformVertex[m].resize(mesh->mNumVertices);

		//ボーンデータ初期化（← 先にやる）
		for (unsigned int b = 0; b < mesh->mNumBones; b++)
		{
			aiBone* bone = mesh->mBones[b];
			m_Bone[bone->mName.C_Str()].OffsetMatrix = bone->mOffsetMatrix;

			for (unsigned int w = 0; w < bone->mNumWeights; w++)
			{
				aiVertexWeight weight = bone->mWeights[w];
				DEFORM_VERTEX& dv = m_DeformVertex[m][weight.mVertexId];
				assert(dv.BoneNum < 4);
				dv.BoneWeight[dv.BoneNum] = weight.mWeight;
				dv.BoneName[dv.BoneNum] = bone->mName.C_Str();
				dv.BoneNum++;
			}
		}

		// 頂点バッファ生成
		{
			VERTEX_SKIN* vertex = new VERTEX_SKIN[mesh->mNumVertices];
			for (unsigned int v = 0; v < mesh->mNumVertices; v++)
			{
				vertex[v].Position = XMFLOAT3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
				vertex[v].Normal = XMFLOAT3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
				vertex[v].TexCoord = XMFLOAT2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
				vertex[v].Diffuse = XMFLOAT4(1, 1, 1, 1);

				DEFORM_VERTEX& dv = m_DeformVertex[m][v];
				for (int b = 0; b < 4; b++)
					vertex[v].BoneIndex[b] = (b < dv.BoneNum) ? m_Bone[dv.BoneName[b]].Index : 0;
				vertex[v].BoneWeight = XMFLOAT4(dv.BoneWeight[0], dv.BoneWeight[1], dv.BoneWeight[2], dv.BoneWeight[3]);
			}

			D3D11_BUFFER_DESC bd{};
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = sizeof(VERTEX_SKIN) * mesh->mNumVertices;
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;   // ← 重要
			bd.CPUAccessFlags = 0;

			D3D11_SUBRESOURCE_DATA sd{};
			sd.pSysMem = vertex;
			Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer[m]);
			delete[] vertex;
		}

		// インデックスバッファ生成
		{
			unsigned int* index = new unsigned int[mesh->mNumFaces * 3];

			for (unsigned int f = 0; f < mesh->mNumFaces; f++)
			{
				const aiFace* face = &mesh->mFaces[f];

				assert(face->mNumIndices == 3);

				index[f * 3 + 0] = face->mIndices[0];
				index[f * 3 + 1] = face->mIndices[1];
				index[f * 3 + 2] = face->mIndices[2];
			}

			D3D11_BUFFER_DESC bd;
			ZeroMemory(&bd, sizeof(bd));
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = sizeof(unsigned int) * mesh->mNumFaces * 3;
			bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
			bd.CPUAccessFlags = 0;

			D3D11_SUBRESOURCE_DATA sd;
			ZeroMemory(&sd, sizeof(sd));
			sd.pSysMem = index;

			Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_IndexBuffer[m]);

			delete[] index;
		}
}

	//テクスチャ読み込み
	for (unsigned int i = 0; i < m_AiScene->mNumTextures; i++)
	{
		aiTexture* aitexture = m_AiScene->mTextures[i];

		ID3D11ShaderResourceView* texture;

		// テクスチャ読み込み
		TexMetadata metadata;
		ScratchImage image;
		LoadFromWICMemory(aitexture->pcData, aitexture->mWidth, WIC_FLAGS_NONE, &metadata, image);
		CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(), image.GetImageCount(), metadata, &texture);
		assert(texture);

		m_Texture[aitexture->mFilename.data] = texture;
	}
}

void AnimationModel::LoadAnimation(const char* FileName, const char* Name)
{

	m_Animation[Name] = aiImportFile(FileName, aiProcess_ConvertToLeftHanded);
	assert(m_Animation[Name]);

}


bool AnimationModel::HasAnimation(const char* Name) const
{
	auto it = m_Animation.find(Name);
	return it != m_Animation.end() && it->second->HasAnimations();
}


void AnimationModel::CreateBone(aiNode* node)
{
	BONE bone;
	bone.Index = (int)m_Bone.size();
	m_Bone[node->mName.C_Str()] = bone;
	assert(m_Bone.size() <=MAX_BONE);
	for(unsigned int n = 0; n < node->mNumChildren; n++) CreateBone(node->mChildren[n]);
}


void AnimationModel::Uninit()
{
	for (unsigned int m = 0; m < m_AiScene->mNumMeshes; m++)
	{
		m_VertexBuffer[m]->Release();
		m_IndexBuffer[m]->Release();
	}

	if (m_BoneBuffer) m_BoneBuffer->Release();

	delete[] m_VertexBuffer;
	delete[] m_IndexBuffer;

	delete[] m_DeformVertex;


	for (std::pair<const std::string, ID3D11ShaderResourceView*> pair : m_Texture)
	{
		pair.second->Release();
	}



	aiReleaseImport(m_AiScene);


	for (std::pair<const std::string, const aiScene*> pair : m_Animation)
	{
		aiReleaseImport(pair.second);
	}

}

void AnimationModel::Update(const char* AnimationName1, int Frame1,
	const char* AnimationName2, int Frame2, float Blend)
{
	// アニメーションがあるか確認
	if (m_Animation.count(AnimationName1) == 0)
		return;

	if (!m_Animation[AnimationName1]->HasAnimations())
		return;

	if (m_Animation.count(AnimationName2) == 0)
		return;

	if (!m_Animation[AnimationName2]->HasAnimations())
		return;

	// アニメーションデータからボーンマトリクスを算出
	aiAnimation* animation1 = m_Animation[AnimationName1]->mAnimations[0];
	aiAnimation* animation2 = m_Animation[AnimationName2]->mAnimations[0];

	m_MatchedBoneNum1 = 0;
	m_MatchedBoneNum2 = 0;

	// 骨の分だけ繰り返す、一個ずつ取り出す
	for (auto pair : m_Bone)
	{
		BONE* bone = &m_Bone[pair.first];

		// 骨に一個分に対応するアニメーション
		aiNodeAnim* nodeAnim1 = nullptr;
		for (unsigned int c = 0; c < animation1->mNumChannels; c++)
		{
			if (animation1->mChannels[c]->mNodeName == aiString(pair.first))
			{
				nodeAnim1 = animation1->mChannels[c];
				break;
			}
		}

		// 骨に一個分に対応するアニメーション
		aiNodeAnim* nodeAnim2 = nullptr;
		for (unsigned int c = 0; c < animation2->mNumChannels; c++)
		{
			if (animation2->mChannels[c]->mNodeName == aiString(pair.first))
			{
				nodeAnim2 = animation2->mChannels[c];
				break;
			}
		}

		// フレーム番号、アニメーションのコマ数
		int f;
		aiQuaternion rot1;
		aiVector3D pos1;

		// 対応したフレーム番号の角度とか位置を取得
		if (nodeAnim1)
		{
			m_MatchedBoneNum1++;

			f = Frame1 % nodeAnim1->mNumRotationKeys;
			rot1 = nodeAnim1->mRotationKeys[f].mValue;

			f = Frame1 % nodeAnim1->mNumPositionKeys;
			pos1 = nodeAnim1->mPositionKeys[f].mValue;
		}

		aiQuaternion rot2;
		aiVector3D pos2;

		// 対応したフレーム番号の角度とか位置を取得
		if (nodeAnim2)
		{
			m_MatchedBoneNum2++;

			f = Frame2 % nodeAnim2->mNumRotationKeys;
			rot2 = nodeAnim2->mRotationKeys[f].mValue;

			f = Frame2 % nodeAnim2->mNumPositionKeys;
			pos2 = nodeAnim2->mPositionKeys[f].mValue;
		}

		/*
		線形補完を使うことによって、Idle→Runなどのアニメーションの切り替えが
		ガクガクせずに滑らかに切り替わるようになる
		*/
		aiVector3D pos = pos1 * (1.0f - Blend) + pos2 * Blend; // 線形補完

		aiQuaternion rot;
		aiQuaternion::Interpolate(rot, rot1, rot2, Blend); // 球面線形補完

		// ボーンマトリクスを作る
		bone->AnimationMatrix = aiMatrix4x4(aiVector3D(1.0f, 1.0f, 1.0f), rot, pos);
	}

	// 再帰的にボーンマトリクスを更新
	aiMatrix4x4 rootMatrix = aiMatrix4x4(aiVector3D(1.0f, 1.0f, 1.0f),
		aiQuaternion((float)AI_MATH_PI, 0.0f, 0.0f), aiVector3D(0.0f, 0.0f, 0.0f));

	// 親子関係を付けてあげる
	UpdateBoneMatrix(m_AiScene->mRootNode, rootMatrix);

	D3D11_MAPPED_SUBRESOURCE ms;
	Renderer::GetDeviceContext()->Map(m_BoneBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
	XMFLOAT4X4*mat=(XMFLOAT4X4*)ms.pData;
	for (auto& pair : m_Bone)	memcpy(&mat[pair.second.Index], &pair.second.Matrix, sizeof(XMFLOAT4X4));

	Renderer::GetDeviceContext()->Unmap(m_BoneBuffer, 0);

}

// nodeはボーンって言ったりノードって言ったり、matrixは親のボーンマトリクス
void AnimationModel::UpdateBoneMatrix(aiNode* node, aiMatrix4x4 matrix)
{
	// 骨一個分を取り出す
	// C_Str()は文字で制御してるから変換している
	BONE* bone = &m_Bone[node->mName.C_Str()];

	// 頭にaiがついているのはassimpのこと
	// assimpのマトリクス転置的(DirectXとは逆)なので行列の掛け算の順番は逆になる
	aiMatrix4x4 worldMatrix;
	// 骨一個分のマトリクスを求める
	worldMatrix = matrix * bone->AnimationMatrix;

	// 骨にそってスキン(皮膚)を求めるのに、今までは中心からの位置を求めている
	// offsetMatrixはそれを直してくれる
	bone->Matrix = worldMatrix * bone->OffsetMatrix;
	for (unsigned int n = 0; n < node->mNumChildren; n++)
	{
		UpdateBoneMatrix(node->mChildren[n], worldMatrix);
	}

}