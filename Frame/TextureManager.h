#pragma once
#include "main.h"
#include <string>
#include <unordered_map>

class TextureManager
{
private:
	static std::unordered_map<std::wstring, ID3D11ShaderResourceView*> m_Pool;

	public:
		static ID3D11ShaderResourceView* Load(const wchar_t* filename);
		static void Unload();
};

