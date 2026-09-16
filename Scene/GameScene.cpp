/*==============================================================================

[GameScene.cpp]
														Author :Watanabe Retsu
														Date   :
--------------------------------------------------------------------------------

==============================================================================*/

//==============================================================================
//インクルード
//==============================================================================
#include "main.h"
#include "Manager.h"
#include "Camera.h"
#include "Field.h"
#include "MeshField.h"
#include "Player.h"
#include "Enemy.h"
#include "Tree.h"
#include "Sky.h"
#include "Box.h"
#include "Score.h"

#include "GameScene.h"
#include "ResultScene.h"

//==============================================================================
//マクロ宣言
//==============================================================================
#define ENEMY_COUNT 1
#define TREE_COUNT 5
//==============================================================================
//プロトタイプ宣言
//==============================================================================

//==============================================================================
//グローバル変数
//==============================================================================

//==============================================================================
//初期化処理
//==============================================================================
void GameScene::Init()
{
	Manager::AddGameObject<CAMERA>();

	Manager::AddGameObject<Sky>();

	//Manager::AddGameObject<FIELD>();
	MeshField* meshField = Manager::AddGameObject<MeshField>();

	Box* box = Manager::AddGameObject<Box>();
	box->SetPosition({ 2.0f, 0.0f, -3.0f });
	box->SetScale({ 1.0f, 1.0f, 1.0f });

	Manager::AddGameObject<Player>();

	////敵の生成
	for (int i = 0; i < ENEMY_COUNT; i++)
	{
		Vector3 pos = { (float)(rand() % 40 - 20),0.0f,(float)(rand() % 40 - 20) };
		pos.y = meshField->GetHeight(pos);	// 起伏の上に足を置く
		Manager::AddGameObject<Enemy>()->SetPosition(pos);
	}


	//木の生成
	for (int i = 0; i < TREE_COUNT; i++)
	{
		Vector3 pos = { (float)(rand() % 40 - 20),0.0f,(float)(rand() % 40 - 20) };
		pos.y = meshField->GetHeight(pos);	// 起伏の上に足を置く
		Manager::AddGameObject<Tree>()->SetPosition(pos);
	}

	//Manager::AddGameObject<Polygon2D>();
	Manager::AddGameObject<Score>()->SetPosition({ 0.0f, 0.0f, 0.0f });

}

//==============================================================================
//終了処理
//==============================================================================
void GameScene::Uninit()
{

}

//==============================================================================
//更新処理
//==============================================================================
void GameScene::Update()
{
	auto enemies = Manager::GetGameObjects<Enemy>();

	/*if (enemies.size() == 0)
	{
		Manager::ChangeScene<ResultScene>();
	}*/
}

//==============================================================================
//描画処理
//==============================================================================
void GameScene::Draw()
{

}