/*==============================================================================

[ShaderManager.cpp]
														Author :Watanabe Retsu
														Date   :2026/09/10
--------------------------------------------------------------------------------
・シェーダの読み込み
・シェーダの解放

==============================================================================*/

//==============================================================================
//インクルード
//==============================================================================
#include "main.h"
#include "Renderer.h"
#include "ShaderManager.h"

std::unordered_map<std::string, ShaderSet> ShaderManager::m_Pool;

//==============================================================================
//　デバイスコンテキストにセット
//==============================================================================
void ShaderSet::Set() const
{
	// デバイスコンテキストに設定する
	ID3D11DeviceContext* g_DeviceContext = Renderer::GetDeviceContext();
	g_DeviceContext->IASetInputLayout(VertexLayout);
	g_DeviceContext->VSSetShader(VertexShader, nullptr, 0);
	g_DeviceContext->PSSetShader(PixelShader, nullptr, 0);
}

//==============================================================================
//シェーダの読み込み
//==============================================================================
const ShaderSet* ShaderManager::Load(const char* vsFile, const char* psFile)
{
	std::string key = std::string(vsFile) + "|" + psFile;

	//読み込み済みならそれを返す
	auto it = m_Pool.find(key);
	if (it != m_Pool.end())
		return &it->second;

	//初回だけファイルから作成
	ShaderSet shader;
	Renderer::CreateVertexShader(&shader.VertexShader, &shader.VertexLayout, vsFile);
	Renderer::CreatePixelShader(&shader.PixelShader, psFile);

	auto result = m_Pool.emplace(key, shader);
	return &result.first->second;
}

//==============================================================================
//シェーダの解放
//==============================================================================
void ShaderManager::Unload()
{
	for (auto& pair : m_Pool)
	{
		ShaderSet& shader = pair.second;
		if (shader.VertexShader) shader.VertexShader->Release();
		if (shader.PixelShader) shader.PixelShader->Release();
		if (shader.VertexLayout) shader.VertexLayout->Release();
	}
	m_Pool.clear();
}