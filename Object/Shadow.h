#pragma once
#include "main.h"
#include "Vector3.h"
#include "GameObject.h"

class Shadow : public GameObject
{
private:
	static const int DIV = 8;	// 影メッシュの分割数（大きいほど地形の起伏に細かく追従する）

	ID3D11RasterizerState* m_RasterState = nullptr;	// 両面描画＋深度バイアス用（Initで1回だけ生成）
	VERTEX_3D m_Vertex[DIV + 1][DIV + 1];			// ワールド座標で保持する影メッシュ

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
