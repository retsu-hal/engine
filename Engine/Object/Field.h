#pragma once
#include "EngineAPI.h"
#include "main.h"
#include "GameObject.h"

class ENGINE_API FIELD : public GameObject
{
private:

public:
	// 頂点構造体
	struct Vertex3D {
		XMFLOAT3 Position;
		XMFLOAT3 Normal;
		XMFLOAT4 Diffuse;
		XMFLOAT2 TexCoord;
	};

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};


