#pragma once
#include "Scene.h"

class GameScene: public Scene
{
public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};

