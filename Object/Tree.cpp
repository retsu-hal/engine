/*==============================================================================

[Tree.cpp]
														Author :Watanabe Retsu
														Date   :
--------------------------------------------------------------------------------

==============================================================================*/

//==============================================================================
//インクルード
//==============================================================================
#include "main.h"
#include "Tree.h"
#include  "BillboardRenderer.h"
#include "SpriteAnimation.h"
#include "Collider.h"
//==============================================================================
//初期化処理
//==============================================================================


void Tree::Init()
{
	m_Layer = 2;
	BillboardRenderer* renderer = AddComponent<BillboardRenderer>(this);
	renderer->Load(L"asset\\texture\\tree.png");
	renderer->SetMode(BillboardMode::AxisY);		// Y軸だけ回転する
	renderer->SetAnchorBottom(true);					// 足元を原点にする
	renderer->SetSize(7.0f, 7.0f);

	BoxCollider* collider = AddComponent<BoxCollider>(this);
	collider->SetSize({1.0f, 7.0f, 1.0f});
	collider->SetOffset({ 0.0f, 3.5f, 0.0f });	
	collider->SetStatic(true);
}