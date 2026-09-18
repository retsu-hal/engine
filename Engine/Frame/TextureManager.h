#pragma once
#include "EngineAPI.h"
#include "main.h"
#include <string>
#include <unordered_map>

class ENGINE_API TextureManager
{
private:
	static std::unordered_map<std::wstring, ID3D11ShaderResourceView*> m_Pool;

	public:
		// 読み込み済みなら同じものを返す（解放は Unload でまとめて行う）
		static ID3D11ShaderResourceView* Load(const wchar_t* filename);
		static void Unload();

		// 画像ファイル（png / jpg / bmp など）から作る。使い終わったら呼んだ側で Release する。失敗したら nullptr
		static ID3D11ShaderResourceView* LoadFromFile(const wchar_t* filename);
		// メモリ上の画像ファイル（FBX に埋め込まれたテクスチャなど）から作る
		static ID3D11ShaderResourceView* LoadFromMemory(const void* data, size_t size);
};

