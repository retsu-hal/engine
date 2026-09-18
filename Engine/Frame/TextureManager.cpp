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
#include <wincodec.h>
#include <vector>
#pragma comment(lib, "windowscodecs.lib")

std::unordered_map<std::wstring, ID3D11ShaderResourceView*> TextureManager::m_Pool;

//==============================================================================
//テクスチャの読み込み
//==============================================================================
ID3D11ShaderResourceView* TextureManager::Load(const wchar_t* filename)
{
	auto it = m_Pool.find(filename);	//読み込み済みか検索
	if (it != m_Pool.end()) return it->second;	//読み込み済みならそれを返す
	
	ID3D11ShaderResourceView* texture = LoadFromFile(filename);	//ファイルから読み込み
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

//==============================================================================
// WIC（Windows 標準の画像読み込み）でテクスチャを作る
// 以前は DirectXTex を使っていたが、DLL 化で CRT の設定を揃える必要があるため Windows 標準の機能に置き換えた
//==============================================================================
static IWICImagingFactory* GetWICFactory()
{
	static IWICImagingFactory* factory = nullptr;
	if (factory == nullptr)
	{
		HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
		if (FAILED(hr)) factory = nullptr;
	}
	return factory;
}

// デコーダー（読み込んだ画像）から RGBA 32bit のテクスチャを作る
static ID3D11ShaderResourceView* CreateFromDecoder(IWICImagingFactory* factory, IWICBitmapDecoder* decoder)
{
	ID3D11ShaderResourceView* result = nullptr;
	IWICBitmapFrameDecode* frame = nullptr;
	IWICFormatConverter* converter = nullptr;

	if (SUCCEEDED(decoder->GetFrame(0, &frame)) &&
		SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
		SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
	{
		UINT width = 0, height = 0;
		converter->GetSize(&width, &height);

		std::vector<BYTE> pixels((size_t)width * height * 4);
		if (width > 0 && height > 0 &&
			SUCCEEDED(converter->CopyPixels(nullptr, width * 4, (UINT)pixels.size(), pixels.data())))
		{
			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = width;
			desc.Height = height;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			D3D11_SUBRESOURCE_DATA initData{};
			initData.pSysMem = pixels.data();
			initData.SysMemPitch = width * 4;

			ID3D11Texture2D* texture = nullptr;
			if (SUCCEEDED(Renderer::GetDevice()->CreateTexture2D(&desc, &initData, &texture)))
			{
				Renderer::GetDevice()->CreateShaderResourceView(texture, nullptr, &result);
				texture->Release();
			}
		}
	}

	if (converter) converter->Release();
	if (frame) frame->Release();
	return result;
}

ID3D11ShaderResourceView* TextureManager::LoadFromFile(const wchar_t* filename)
{
	IWICImagingFactory* factory = GetWICFactory();
	if (factory == nullptr || filename == nullptr || filename[0] == L'\0') return nullptr;	// テクスチャなしのマテリアル

	IWICBitmapDecoder* decoder = nullptr;
	if (FAILED(factory->CreateDecoderFromFilename(filename, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)))
	{
		OutputDebugStringW(L"テクスチャが読み込めない: ");
		OutputDebugStringW(filename);
		OutputDebugStringW(L"\n");
		return nullptr;
	}

	ID3D11ShaderResourceView* result = CreateFromDecoder(factory, decoder);
	decoder->Release();
	return result;
}

ID3D11ShaderResourceView* TextureManager::LoadFromMemory(const void* data, size_t size)
{
	IWICImagingFactory* factory = GetWICFactory();
	if (factory == nullptr || data == nullptr || size == 0) return nullptr;

	ID3D11ShaderResourceView* result = nullptr;
	IWICStream* stream = nullptr;
	IWICBitmapDecoder* decoder = nullptr;
	if (SUCCEEDED(factory->CreateStream(&stream)) &&
		SUCCEEDED(stream->InitializeFromMemory((BYTE*)data, (DWORD)size)) &&
		SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder)))
	{
		result = CreateFromDecoder(factory, decoder);
	}
	if (decoder) decoder->Release();
	if (stream) stream->Release();
	return result;
}
