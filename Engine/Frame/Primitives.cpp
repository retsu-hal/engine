#include "main.h"
#include "Manager.h"
#include "GameObject.h"
#include "Primitives.h"
#include "PrimitiveRenderer.h"
#include "Collider.h"
#include "CameraComponent.h"

namespace Primitives
{

GameObject* CreateEmpty(const Vector3& position)
{
	GameObject* object = Manager::CreateGameObject("GameObject");
	object->SetPosition(position);
	return object;
}

GameObject* CreateCube(const Vector3& position)
{
	GameObject* object = Manager::CreateGameObject("Cube");
	object->SetPosition(position);
	object->AddComponent<PrimitiveRenderer>()->SetShape(PrimitiveRenderer::Shape::Cube);

	BoxCollider* collider = object->AddComponent<BoxCollider>();
	collider->SetSize({ 1.0f, 1.0f, 1.0f });
	collider->SetStatic(true);
	return object;
}

GameObject* CreateSphere(const Vector3& position)
{
	GameObject* object = Manager::CreateGameObject("Sphere");
	object->SetPosition(position);
	object->AddComponent<PrimitiveRenderer>()->SetShape(PrimitiveRenderer::Shape::Sphere);

	SphereCollider* collider = object->AddComponent<SphereCollider>();
	collider->SetRadius(0.5f);
	collider->SetStatic(true);
	return object;
}

GameObject* CreateCapsule(const Vector3& position)
{
	GameObject* object = Manager::CreateGameObject("Capsule");
	object->SetPosition(position);
	object->AddComponent<PrimitiveRenderer>()->SetShape(PrimitiveRenderer::Shape::Capsule);

	CapsuleCollider* collider = object->AddComponent<CapsuleCollider>();
	collider->SetRadius(0.5f);
	collider->SetHeight(2.0f);
	collider->SetStatic(true);
	return object;
}

GameObject* CreateCamera(const Vector3& position)
{
	GameObject* object = Manager::CreateGameObject("Camera");
	object->SetPosition(position);
	object->AddComponent<CameraComponent>();
	return object;
}

}
