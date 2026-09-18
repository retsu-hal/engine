#pragma once
#include "EngineAPI.h" 
//================================================================
//	インクルード
//================================================================
#include"gameObject.h"

//================================================================
//	構造体
//================================================================
class ENGINE_API MeshField : public GameObject
{
private:
    VERTEX_3D m_Vertex[21][21];
public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    float GetHeight(Vector3 position);
};
