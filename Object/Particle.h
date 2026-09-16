#pragma once
#include "GameObject.h"
class Particle :  public GameObject
{
private:

	struct PARTICLE
	{
		Vector3 Position;
		Vector3 Velocity;
		int  Life;
		bool Enable;
	};

	static const int PARTICLE_MAX = 10000;
	PARTICLE m_Particle[PARTICLE_MAX];

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};

