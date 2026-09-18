#pragma once
#include "EngineAPI.h"
#include "GameObject.h"
class ENGINE_API Particle :  public GameObject
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

