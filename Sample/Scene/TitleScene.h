#pragma once
#include "Scene.h"
class TitleScene :public Scene
{
protected:
	class Polygon2D* m_Logo;
	float m_logoX;
	float m_logoY;

	class Polygon2D* m_startButton;
	float m_startX;
	float m_startY;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};

