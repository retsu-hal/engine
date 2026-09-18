#pragma once
#include "EngineAPI.h"

class ENGINE_API Scene
{
public:
	virtual void Init() {};
	virtual void Uninit() {};
	virtual void Update() {};
	virtual void Draw() {};	
};
