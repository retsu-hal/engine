/*==============================================================================

[Explosion.cpp]
														Author :Watanabe Retsu
														Date   :
--------------------------------------------------------------------------------

==============================================================================*/

//==============================================================================
//インクルード
//==============================================================================
#include "main.h"
#include "Renderer.h"
#include "Explosion.h"
#include "BillboardRenderer.h"
#include "SpriteAnimation.h"

//==============================================================================
//初期化処理
//==============================================================================
void Explosion::Init()
{
	m_Layer = 2;
	m_Scale = Vector3(1.0f, 1.0f, 1.0f);

	BillboardRenderer* renderer = AddComponent<BillboardRenderer>(this);
	renderer->Load(L"asset\\texture\\Explosion.png");

	//4×4分割、16コマ、60コマ/秒、再生後に破棄
	AddComponent<SpriteAnimation>(this)->Setup(renderer, 4, 4, 16, 60.0f);
}

