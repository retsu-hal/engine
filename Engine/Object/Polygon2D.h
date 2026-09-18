#pragma once
#include "EngineAPI.h"

#include "GameObject.h"

class ENGINE_API Polygon2D : public GameObject
{
public:
	void Init() override{}
	void Init(float x, float y, float width, float height, const WCHAR* TextureName);
	void Uninit() override;
	void Update() override;
	void Draw() override	;
};