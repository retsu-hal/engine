#pragma once
#include "EngineAPI.h"

#include <unordered_map>

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/matrix4x4.h"
#pragma comment (lib, "assimp-vc143-mt.lib")

#include "Component.h"

#define MAX_BONE 256

//変形後頂点構造体
struct DEFORM_VERTEX
{
	aiVector3D Position;
	aiVector3D Normal;
	int				BoneNum = 0;
	std::string		BoneName[4];//本来はボーンインデックスで管理するべき
	float			BoneWeight[4]{};
};

//ボーン構造体
struct BONE
{
	aiMatrix4x4 Matrix;
	aiMatrix4x4 AnimationMatrix;
	aiMatrix4x4 OffsetMatrix;
	int Index = 0;
};

//前方宣言
struct ShaderSet;

class ENGINE_API AnimationModel : public Component
{
private:
	const aiScene* m_AiScene = nullptr;
	std::unordered_map<std::string, const aiScene*> m_Animation;

	ID3D11Buffer**	m_VertexBuffer = nullptr;
	ID3D11Buffer**	m_IndexBuffer = nullptr;

	std::unordered_map<std::string, ID3D11ShaderResourceView*> m_Texture;

	std::vector<DEFORM_VERTEX>* m_DeformVertex = nullptr;//変形後頂点データ
	std::unordered_map<std::string, BONE> m_Bone;//ボーンデータ（名前で参照）

	//直近のUpdateでアニメーションチャンネルが見つかったボーン数（デバッグ用）
	int m_MatchedBoneNum1 = 0;
	int m_MatchedBoneNum2 = 0;

	void CreateBone(aiNode* Node);
	void UpdateBoneMatrix(aiNode* Node, aiMatrix4x4 Matrix);

	const ShaderSet* m_Shader = nullptr;

	ID3D11Buffer* m_BoneBuffer = nullptr;

	// 保存用
	std::string m_FileName;
	std::vector<std::pair<std::string, std::string>> m_AnimationFiles;	// 名前, ファイル

public:
	using Component::Component;

	void Load( const char *FileName );
	void LoadAnimation( const char *FileName, const char *Name);
	void Uninit() override;
	void Update(const char* AnimationName1, int Frame1, 
		const char* AnimationName2, int Frame2, float Blend);
	void Draw() override;

	void SetShader(const char* vsFile, const char* psFile);

	void OnInspectorGUI() override;
	void Serialize(nlohmann::json& data) const override;
	void Deserialize(const nlohmann::json& data) override;


	//----- デバッグ用 -----
	bool HasAnimation(const char* Name) const;
	int  GetBoneNum() const { return (int)m_Bone.size(); }
	int  GetMatchedBoneNum1() const { return m_MatchedBoneNum1; }
	int  GetMatchedBoneNum2() const { return m_MatchedBoneNum2; }
};