#pragma once
#include "main.h"
#include <string>
#include <unordered_map>

struct ShaderSet
{

	ID3D11VertexShader* VertexShader = nullptr;
	ID3D11PixelShader* PixelShader = nullptr;
	ID3D11InputLayout* VertexLayout = nullptr;

	void Set() const;	// デバイスコンテキストに設定する
};

class ShaderManager
{
private:
	static std::unordered_map<std::string, ShaderSet> m_Pool;

public:
	static const ShaderSet* Load(const char* vsFile, const char* psFile);
	static void Unload();

};

