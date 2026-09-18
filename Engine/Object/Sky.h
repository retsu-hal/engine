#pragma once
#include "EngineAPI.h"
#include "GameObject.h"
class ENGINE_API Sky :public GameObject
{
public:
	void Init() override;
	void Update() override;
};

