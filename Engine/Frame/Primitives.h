#pragma once
#include "EngineAPI.h"
#include "Vector3.h"

class GameObject;

// Hierarchy の右クリック（GameObject メニュー）から作る基本の形（当たり判定付き）
namespace Primitives
{
	ENGINE_API GameObject* CreateEmpty(const Vector3& position);
	ENGINE_API GameObject* CreateCube(const Vector3& position);
	ENGINE_API GameObject* CreateSphere(const Vector3& position);
	ENGINE_API GameObject* CreateCapsule(const Vector3& position);
	ENGINE_API GameObject* CreateCamera(const Vector3& position);
}
