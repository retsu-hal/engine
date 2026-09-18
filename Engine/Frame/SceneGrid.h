#pragma once
#include "EngineAPI.h"
#include "Vector3.h"

// Scene ビューの床のグリッド（高さ 0 の面に 1m 間隔の線。10m ごとに少し明るく、遠くは薄く）
class ENGINE_API SceneGrid
{
private:
	static ID3D11Buffer* m_VertexBuffer;
	static UINT          m_Capacity;
	static bool          m_Enable;

public:
	static void Uninit();
	static void Draw(const Vector3& cameraPosition);	// 行列を設定してから呼ぶ

	static void SetEnable(bool enable) { m_Enable = enable; }
	static bool IsEnable() { return m_Enable; }
	static bool* GetEnablePtr() { return &m_Enable; }
};
