#include "main.h"
#include "Renderer.h"
#include "Manager.h"
#include "GameObject.h"
#include "Prefabs.h"

#include "ModelRenderer.h"
#include "AnimationModel.h"
#include "BillboardRenderer.h"
#include "SpriteAnimation.h"
#include "Collider.h"
#include "Rigidbody.h"
#include "Audio.h"
#include "Shadow.h"
#include "PrimitiveRenderer.h"
#include "CameraComponent.h"

#include "PlayerController.h"
#include "EnemyController.h"
#include "BulletController.h"

namespace Prefabs
{

GameObject* CreateBox(const Vector3& position)
{
	GameObject* object = Manager::CreateGameObject("Box");
	object->SetPosition(position);

	object->AddComponent<ModelRenderer>()->Load("asset\\model\\box.obj");

	BoxCollider* collider = object->AddComponent<BoxCollider>();
	collider->SetSize({ 2.0f, 2.0f, 2.0f });
	collider->SetOffset({ 0.0f, 1.0f, 0.0f });
	collider->SetStatic(true);

	return object;
}

GameObject* CreateTree(const Vector3& position)
{
	GameObject* object = Manager::CreateGameObject("Tree");
	object->SetPosition(position);
	object->SetLayer(2);

	BillboardRenderer* renderer = object->AddComponent<BillboardRenderer>();
	renderer->Load(L"asset\\texture\\tree.png");
	renderer->SetMode(BillboardMode::AxisY);	// Y軸だけ回転する
	renderer->SetAnchorBottom(true);			// 足元を原点にする
	renderer->SetSize(7.0f, 7.0f);

	BoxCollider* collider = object->AddComponent<BoxCollider>();
	collider->SetSize({ 1.0f, 7.0f, 1.0f });
	collider->SetOffset({ 0.0f, 3.5f, 0.0f });
	collider->SetStatic(true);

	return object;
}

GameObject* CreateExplosion(const Vector3& position, const Vector3& scale)
{
	GameObject* object = Manager::CreateGameObject("Explosion");
	object->SetPosition(position);
	object->SetScale(scale);
	object->SetLayer(2);

	BillboardRenderer* renderer = object->AddComponent<BillboardRenderer>();
	renderer->Load(L"asset\\texture\\Explosion.png");

	//4×4分割、16コマ、60コマ/秒、再生後に破棄
	object->AddComponent<SpriteAnimation>()->Setup(renderer, 4, 4, 16, 60.0f);

	return object;
}

GameObject* CreateBullet(const Vector3& position, const Vector3& velocity)
{
	GameObject* object = Manager::CreateGameObject("Bullet");
	object->SetPosition(position);

	object->AddComponent<ModelRenderer>()->Load("asset\\model\\bullet.obj");

	SphereCollider* collider = object->AddComponent<SphereCollider>();
	collider->SetRadius(0.3f);
	collider->SetTrigger(true);

	Rigidbody* rigidbody = object->AddComponent<Rigidbody>();
	rigidbody->SetUseGravity(false);
	rigidbody->SetUseGround(false);
	rigidbody->SetVelocity(velocity);

	object->AddComponent<BulletController>();

	return object;
}

GameObject* CreateEnemy(const Vector3& position)
{
	GameObject* object = Manager::CreateGameObject("Enemy");
	object->SetPosition(position);

	// 操作スクリプトは先頭に付ける（Rigidbody より先に Update させるため）
	object->AddComponent<EnemyController>();

	object->AddComponent<ModelRenderer>()->Load("asset\\model\\player.obj");

	CapsuleCollider* collider = object->AddComponent<CapsuleCollider>();
	collider->SetRadius(0.5f);
	collider->SetHeight(2.0f);
	collider->SetOffset({ 0.0f, 1.0f, 0.0f });

	Rigidbody* rigidbody = object->AddComponent<Rigidbody>();
	rigidbody->SetGravity(40.0f);
	rigidbody->SetDrag(5.0f);

	return object;
}

GameObject* CreatePlayer(const Vector3& position)
{
	GameObject* object = Manager::CreateGameObject("Player");
	object->SetTag("Player");	// カメラ（CAMERA）が Tag で探して追いかける
	object->SetPosition(position);
	object->SetScale({ 0.01f, 0.01f, 0.01f });

	// 操作スクリプトは先頭に付ける（Rigidbody より先に Update させるため）
	object->AddComponent<PlayerController>();

	AnimationModel* model = object->AddComponent<AnimationModel>();
	model->Load("asset\\model\\Akai.fbx");
	model->LoadAnimation("asset\\model\\Akai_Idle.fbx", "Idle");
	model->LoadAnimation("asset\\model\\Akai_Run.fbx", "Run");

	CapsuleCollider* collider = object->AddComponent<CapsuleCollider>();
	collider->SetRadius(40.0f);
	collider->SetHeight(180.0f);
	collider->SetOffset({ 0.0f, 90.0f, 0.0f });

	Rigidbody* rigidbody = object->AddComponent<Rigidbody>();
	rigidbody->SetGravity(40.0f);
	rigidbody->SetDrag(5.0f);

	object->AddComponent<Audio>()->Load("asset\\audio\\wan.wav");

	// 影はプレイヤーの子にする（プレイヤーが消えると一緒に消える）
	// スケールはプレイヤーの 0.01 を受けないよう、Shadow 側は自分の m_Scale だけを使う
	Shadow* shadow = Manager::AddGameObject<Shadow>();
	shadow->SetScale({ 1.5f, 1.5f, 1.5f });
	shadow->SetParent(object);

	return object;
}

}
