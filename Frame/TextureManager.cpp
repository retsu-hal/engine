/*==============================================================================

[TextureManager.cpp]
														Author :Watanabe Retsu
														Date   :2026/09/10
--------------------------------------------------------------------------------
・テクスチャの読み込み
・テクスチャの解放

==============================================================================*/

//==============================================================================
//インクルード
//==============================================================================
#include "TextureManager.h"
#include "Renderer.h"

std::unordered_map<std::wstring, ID3D11ShaderResourceView*> TextureManager::m_Pool;

//==============================================================================
//テクスチャの読み込み
//==============================================================================
ID3D11ShaderResourceView* TextureManager::Load(const wchar_t* filename)
{
	auto it = m_Pool.find(filename);	//読み込み済みか検索
	if (it != m_Pool.end()) return it->second;	//読み込み済みならそれを返す
	
	TexMetadata metadata;
	ScratchImage image;
	LoadFromWICFile(filename, WIC_FLAGS_NONE, &metadata, image);		//ファイルから読み込み

	ID3D11ShaderResourceView* texture = nullptr;
	CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(), image.GetImageCount(), metadata, &texture);	//テクスチャ作成
	assert(texture);	//読み込み失敗時にダイアログを表示

	m_Pool[filename] = texture;
	return texture;
}


//==============================================================================
//テクスチャの解放
//==============================================================================
void TextureManager::Unload()
{
	for (auto& pair : m_Pool)
	{
		if (pair.second) pair.second->Release();
	}
	m_Pool.clear();
}