#pragma once
#include "Vector3.h"

class GameObject;

// よく使うオブジェクトの「組み立て方」をまとめたもの（Unity のプレハブの代わり）
// 以前の Box / Tree / Player などのクラスは、ここで GameObject にコンポーネントを付ける形に置き換えた
namespace Prefabs
{
	GameObject* CreateBox(const Vector3& position);
	GameObject* CreateTree(const Vector3& position);
	GameObject* CreateExplosion(const Vector3& position, const Vector3& scale);
	GameObject* CreateBullet(const Vector3& position, const Vector3& velocity);
	GameObject* CreateEnemy(const Vector3& position);
	GameObject* CreatePlayer(const Vector3& position);
}
