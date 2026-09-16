#pragma once
#include <vector>
#include "Component.h"
#include "Vector3.h"

//判定用の形データ
//すべての形を「上から見た形（XZ）」「高さ（Y）」「丸み」の組み合わせで表す
struct ColliderShape
{
	Vector3 Center{ 0.0f, 0.0f, 0.0f };	//中心（ワールド座標）
	float HalfX = 0.0f;	//上から見た四角の半分の幅（四角以外は0）
	float HalfZ = 0.0f;	//上から見た四角の半分の奥行き（四角以外は0）
	float RadiusXZ = 0.0f;	//上から見た円の半径（円柱のみ）
	float HalfY = 0.0f;	//高さの半分（丸み部分を除く）
	float Radius = 0.0f;	//全方向の丸み（球・カプセルのみ）
};

//コライダーの基底
class Collider : public Component
{
private:
	static std::vector<Collider*> m_List;	//存在する全コライダー

	Vector3 m_Offset{ 0.0f, 0.0f, 0.0f };	//オブジェクトの位置からのずれ
	bool m_IsTrigger = false;	//true: すり抜けて通知だけ
	bool m_IsStatic = false;	//true: 押し出されない
	bool m_Hit = false;	//このフレームに何かと当たったか（可視化用）

protected:
	Vector3 GetCenter() const;	//オフセット込みの中心

public:
	Collider(GameObject* object);
	~Collider() override;

	void SetOffset(const Vector3& offset) { m_Offset = offset; }
	void SetTrigger(bool trigger) { m_IsTrigger = trigger; }
	void SetStatic(bool isStatic) { m_IsStatic = isStatic; }

	virtual ColliderShape GetShape() const = 0;	//形ごとに実装

	static void Check();		//全コライダーの当たり判定
	static void DrawGizmo();	//全コライダーの形をデバッグ表示（ImGui::NewFrame と Gizmo::Draw の間で呼ぶ）
};

//球
class SphereCollider : public Collider
{
private:
	float m_Radius = 0.5f;

public:
	using Collider::Collider;

	void SetRadius(float radius) { m_Radius = radius; }
	ColliderShape GetShape() const override;
};

//箱（回転しない）
class BoxCollider : public Collider
{
private:
	Vector3 m_Size{ 1.0f, 1.0f, 1.0f };	//幅・高さ・奥行き

public:
	using Collider::Collider;

	void SetSize(const Vector3& size) { m_Size = size; }
	ColliderShape GetShape() const override;
};

//カプセル（縦向き）
class CapsuleCollider : public Collider
{
private:
	float m_Radius = 0.5f;
	float m_Height = 2.0f;	//丸い部分を含めた全体の高さ（Unityと同じ）

public:
	using Collider::Collider;

	void SetRadius(float radius) { m_Radius = radius; }
	void SetHeight(float height) { m_Height = height; }
	ColliderShape GetShape() const override;
};

