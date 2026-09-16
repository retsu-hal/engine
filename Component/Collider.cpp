#include "main.h"
#include <algorithm>
#include <typeinfo>
#include "GameObject.h"
#include "Collider.h"
#include "Gizmo.h"
#include "Rigidbody.h"

std::vector<Collider*> Collider::m_List;

//=============================================================
// 登録・解除
//=============================================================
Collider::Collider(GameObject* object)
	: Component(object)
{
	m_List.push_back(this);
}

Collider::~Collider()
{
	m_List.erase(std::remove(m_List.begin(), m_List.end(), this), m_List.end());
}

Vector3 Collider::GetCenter() const
{
	Vector3 pos = m_GameObject->GetPosition();
	Vector3 scale = m_GameObject->GetScale();
	return Vector3(pos.x + m_Offset.x * scale.x,
		pos.y + m_Offset.y * scale.y,
		pos.z + m_Offset.z * scale.z);
}

//=============================================================
// 形ごとのデータ（オブジェクトのスケールを反映）
//=============================================================
ColliderShape SphereCollider::GetShape() const
{
	Vector3 s = m_GameObject->GetScale();

	ColliderShape shape;
	shape.Center = GetCenter();
	shape.Radius = m_Radius * std::max(s.x, std::max(s.y, s.z));
	return shape;
}

ColliderShape BoxCollider::GetShape() const
{
	Vector3 s = m_GameObject->GetScale();

	ColliderShape shape;
	shape.Center = GetCenter();
	shape.HalfX = m_Size.x * s.x * 0.5f;
	shape.HalfY = m_Size.y * s.y * 0.5f;
	shape.HalfZ = m_Size.z * s.z * 0.5f;
	return shape;
}

ColliderShape CapsuleCollider::GetShape() const
{
	Vector3 s = m_GameObject->GetScale();
	float radius = m_Radius * std::max(s.x, s.z);
	float height = m_Height * s.y;

	ColliderShape shape;
	shape.Center = GetCenter();
	shape.Radius = radius;
	shape.HalfY = std::max(0.0f, height * 0.5f - radius);	//高さが直径より低ければ球になる
	return shape;
}

//=============================================================
// 当たり判定（push には「a を押し出す量」を入れる）
//=============================================================
static float Sign(float v) { return v < 0.0f ? -1.0f : 1.0f; }

static bool Hit(const ColliderShape& a, const ColliderShape& b, Vector3& push)
{
	float dx = a.Center.x - b.Center.x;
	float dy = a.Center.y - b.Center.y;
	float dz = a.Center.z - b.Center.z;

	//--- 高さ方向（Y）のすき間（マイナスなら重なっている）
	float gapY = fabsf(dy) - (a.HalfY + b.HalfY);

	//--- 上から見た（XZ）すき間と押し出す向き
	float gx = fabsf(dx) - (a.HalfX + b.HalfX);
	float gz = fabsf(dz) - (a.HalfZ + b.HalfZ);
	float gapXZ, dirX, dirZ;
	if (gx > 0.0f && gz > 0.0f)	//斜めに離れている
	{
		gapXZ = sqrtf(gx * gx + gz * gz);
		dirX = Sign(dx) * gx / gapXZ;
		dirZ = Sign(dz) * gz / gapXZ;
	}
	else if (gx > gz)			//X方向のめり込みが浅い
	{
		gapXZ = gx;  dirX = Sign(dx);  dirZ = 0.0f;
	}
	else						//Z方向のめり込みが浅い
	{
		gapXZ = gz;  dirX = 0.0f;  dirZ = Sign(dz);
	}
	gapXZ -= a.RadiusXZ + b.RadiusXZ;	//円柱の半径ぶん近づける

	//--- 丸み（球・カプセルの半径）を考慮して判定
	float r = a.Radius + b.Radius;

	if (gapXZ > 0.0f && gapY > 0.0f)	//角同士が斜めに向き合っている
	{
		float dist = sqrtf(gapXZ * gapXZ + gapY * gapY);
		if (dist >= r) return false;

		float s = (r - dist) / dist;
		push = Vector3(dirX * gapXZ * s, Sign(dy) * gapY * s, dirZ * gapXZ * s);
		return true;
	}

	if (gapXZ >= r || gapY >= r) return false;

	//めり込みが浅い方向へ押し出す
	if (gapXZ > gapY) push = Vector3(dirX * (r - gapXZ), 0.0f, dirZ * (r - gapXZ));
	else              push = Vector3(0.0f, Sign(dy) * (r - gapY), 0.0f);
	return true;
}

//位置を押し戻して、Rigidbody とオブジェクトに知らせる
static void Push(GameObject* obj, const Vector3& push)
{
	obj->SetPosition(obj->GetPosition() + push);

	if (Rigidbody* rb = obj->GetComponent<Rigidbody>())
		rb->OnPushed(push);

	obj->OnPushed(push);	//独自の反応をしたいオブジェクト用
}


//=============================================================
// 全体の判定
//=============================================================
void Collider::Check()
{
	//判定中にコライダーが増えても壊れないようにコピーを使う
	std::vector<Collider*> list = m_List;

	//当たり表示（Gizmoの色分け用）は毎フレーム作り直す
	for (Collider* collider : list) collider->m_Hit = false;

	for (size_t i = 0; i < list.size(); i++)
	{
		for (size_t j = i + 1; j < list.size(); j++)
		{
			Collider* a = list[i];
			Collider* b = list[j];
			GameObject* objA = a->m_GameObject;
			GameObject* objB = b->m_GameObject;

			if (objA == objB) continue;									//同じオブジェクト同士
			if (objA->IsDestroyed() || objB->IsDestroyed()) continue;	//消える予定のもの
			if (a->m_IsStatic && b->m_IsStatic) continue;				//動かないもの同士

			Vector3 push;
			if (!Hit(a->GetShape(), b->GetShape(), push)) continue;

			a->m_Hit = true;
			b->m_Hit = true;

			objA->OnCollision(objB);
			objB->OnCollision(objA);

			//どちらもトリガーでなければ押し出す
			if (!a->m_IsTrigger && !b->m_IsTrigger)
			{
				if (b->m_IsStatic)      Push(objA, push);
				else if (a->m_IsStatic) Push(objB, push * -1.0f);
				else	//両方動くなら半分ずつ
				{
					Push(objA, push * 0.5f);
					Push(objB, push * -0.5f);
				}
			}
		}
	}
}
//=============================================================
// デバッグ表示
// 判定に使っている GetShape() の値をそのまま描くので、
// 「見えている形＝当たっている形」になり、設定ミスがそのまま目で分かる
//=============================================================
void Collider::DrawGizmo()
{
	if (!Gizmo::IsEnable()) return;

	const XMFLOAT4 COLOR_DYNAMIC{ 0.2f, 1.0f, 0.3f, 1.0f };	//押し出される物＝緑
	const XMFLOAT4 COLOR_STATIC { 0.3f, 0.6f, 1.0f, 1.0f };	//動かない物＝青
	const XMFLOAT4 COLOR_TRIGGER{ 1.0f, 0.9f, 0.2f, 1.0f };	//トリガー＝黄
	const XMFLOAT4 COLOR_HIT    { 1.0f, 0.2f, 0.2f, 1.0f };	//当たっている＝赤

	for (Collider* collider : m_List)
	{
		if (collider->m_GameObject->IsDestroyed()) continue;

		ColliderShape shape = collider->GetShape();

		XMFLOAT4 color = COLOR_DYNAMIC;
		if (collider->m_IsTrigger)     color = COLOR_TRIGGER;
		else if (collider->m_IsStatic) color = COLOR_STATIC;
		if (collider->m_Hit)           color = COLOR_HIT;

		//ColliderShape の中身から形を判断する（Hit() の場合分けと同じ考え方）
		if (shape.Radius > 0.0f)
		{
			if (shape.HalfY > 0.0f) Gizmo::DrawWireCapsule(shape.Center, shape.Radius, shape.HalfY, color);
			else                    Gizmo::DrawWireSphere(shape.Center, shape.Radius, color);
		}
		else if (shape.RadiusXZ > 0.0f)
		{
			Gizmo::DrawWireCylinder(shape.Center, shape.RadiusXZ, shape.HalfY, color);
		}
		else
		{
			Gizmo::DrawWireBox(shape.Center, Vector3(shape.HalfX, shape.HalfY, shape.HalfZ), color);
		}

		//どのオブジェクトのコライダーか分かるように名前を出す（"class Player" の class を取る）
		const char* name = typeid(*collider->m_GameObject).name();
		if (strncmp(name, "class ", 6) == 0) name += 6;
		Gizmo::DrawLabel(shape.Center, name, color);
	}
}
