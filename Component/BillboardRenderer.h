#pragma once
#include "Component.h"

struct ShaderSet;

enum class BillboardMode
{
	Full,	// カメラに完全に正対（爆発・パーティクル）
	AxisY,	// Y軸だけ回転（木・草など地面に立つもの）
};

class BillboardRenderer : public Component
{
private:
	const ShaderSet* m_Shader = nullptr;		// ShaderManagerが所有
	ID3D11ShaderResourceView* m_Texture = nullptr;		// TextureManagerが所有
	ID3D11Buffer* m_VertexBuffer = nullptr;	// このコンポーネント専用

	BillboardMode m_Mode = BillboardMode::Full;
	float    m_Width = 1.0f;
	float    m_Height = 1.0f;
	bool     m_AnchorBottom = false;		// true: 足元を原点にする（木など）
	bool     m_UseATC = false;				// true: アルファトゥカバレッジを使う
	XMFLOAT4 m_Color = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT4 m_UV = { 0.0f, 0.0f, 1.0f, 1.0f };	// u, v, 幅, 高さ

public:
	using Component::Component;	// Component(GameObject*) を引き継ぐ

	void Load(const wchar_t* textureFile);
	void Uninit() override;
	void Draw() override;

	void SetMode(BillboardMode mode) { m_Mode = mode; }
	void SetSize(float width, float height) { m_Width = width; m_Height = height; }
	void SetAnchorBottom(bool enable) { m_AnchorBottom = enable; }
	void SetATC(bool enable) { m_UseATC = enable; }
	void SetColor(const XMFLOAT4& color) { m_Color = color; }
	void SetUV(float u, float v, float w, float h) { m_UV = { u, v, w, h }; }
};