#pragma once

#include "main.h"
#include "Vector3.h"
#include "Component.h"
#include <string>
#include <vector>
#include <algorithm>

class GameObject
{
private:
	// 起動中に重複しない ID を発行する
	static unsigned int NewID()
	{
		static unsigned int next = 1;
		return next++;
	}

	unsigned int m_ID = NewID();
	std::string  m_Name = "GameObject";

	GameObject* m_Parent = nullptr;	// 親（いなければ nullptr）
	std::vector<GameObject*> m_Children;			// 子

protected://外部からアクセスできないが、継承したクラスからアクセスできる
	Vector3 m_Position{ 0.0f, 0.0f, 0.0f };	// 親から見た位置（親がいなければワールド座標）
	Vector3 m_Rotation{ 0.0f, 0.0f, 0.0f };
	Vector3 m_Scale{ 1.0f, 1.0f, 1.0f };
	bool m_Destroy = false;
	int m_Layer = 1;	// レイヤー番号
	float m_CameraZ = 0.0f;	//ソート用Z値

	ID3D11Buffer* m_vertexBuffer = nullptr;	// 頂点バッファ
	ID3D11InputLayout* m_VertexLayout = nullptr;	// 頂点レイアウト
	ID3D11VertexShader* m_VertexShader = nullptr;	// 頂点シェーダー
	ID3D11PixelShader* m_PixelShader = nullptr;	// ピクセルシェーダー
	ID3D11Buffer* m_indexBuffer = nullptr;	// インデックスバッファ
	ID3D11ShaderResourceView* m_Texture = nullptr;		// テクスチャ

	std::list<Component*> m_Components;

public:
	//----------------------------------------------------------
	// 名前・ID
	//----------------------------------------------------------
	unsigned int GetID() const { return m_ID; }
	const std::string& GetName() const { return m_Name; }
	void SetName(const std::string& name) { m_Name = name; }

	//----------------------------------------------------------
	// Transform
	//----------------------------------------------------------
	void SetPosition(const Vector3& position) { m_Position = position; }
	Vector3 GetPosition() const { return m_Position; }
	void SetRotation(const Vector3& rotation) { m_Rotation = rotation; }
	Vector3 GetRotation() const { return m_Rotation; }
	void SetScale(const Vector3& scale) { m_Scale = scale; }
	Vector3 GetScale() const { return m_Scale; }

	//----------------------------------------------------------
	// 親子関係
	//----------------------------------------------------------
	GameObject* GetParent() const { return m_Parent; }
	const std::vector<GameObject*>& GetChildren() const { return m_Children; }

	// parent に nullptr を渡すと親子関係を解除する
	// ※ m_Position はそのまま「親から見た位置」になるので、見た目の位置は変わることがある
	void SetParent(GameObject* parent)
	{
		if (parent == m_Parent) return;

		// 自分や自分の子孫を親にすると循環するので禁止
		for (GameObject* p = parent; p != nullptr; p = p->m_Parent)
		{
			if (p == this) return;
		}

		if (m_Parent)
		{
			auto& siblings = m_Parent->m_Children;
			siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
		}

		m_Parent = parent;
		if (m_Parent) m_Parent->m_Children.push_back(this);
	}

	//----------------------------------------------------------
	// 破棄
	//----------------------------------------------------------
	void SetDestroy() { m_Destroy = true; }
	bool IsDestroyed() const { return m_Destroy; }

	virtual void OnCollision(GameObject* other) {}

	// 当たり判定から呼ぶ。オブジェクト自身と、全コンポーネントに通知する
	void NotifyCollision(GameObject* other)
	{
		OnCollision(other);
		for (Component* component : m_Components)
		{
			if (component != nullptr && component->IsEnabled()) component->OnCollision(other);
		}
	}
	virtual void OnPushed(const Vector3& push) {}
	int GetLayer() { return m_Layer; }
	void SetLayer(int layer) { m_Layer = layer; }
	float GetCameraZ() const { return m_CameraZ; }
	void CalcCameraZ(Vector3 CameraPos, Vector3 CameraForward)
	{
		Vector3 dir = GetWorldPosition() - CameraPos;
		m_CameraZ = Vector3::dot(dir, CameraForward);	//内積
	}

	// Uninit を上書きして基底を呼ばないクラスがあっても確実に外れるよう、デストラクタで親子を切る
	virtual ~GameObject()
	{
		// 子は一緒に破棄する（Unity と同じ）
		for (GameObject* child : m_Children)
		{
			child->m_Parent = nullptr;
			child->SetDestroy();
		}
		m_Children.clear();

		SetParent(nullptr);
	}

	virtual void Init() {};
	virtual void Uninit()
	{
		for (Component* component : m_Components)
		{
			if (component != nullptr)
			{
				component->Uninit();
				delete component;
			}
		}
		m_Components.clear();
	};

	virtual void Update()
	{
		for (Component* component : m_Components)
		{
			if (component != nullptr && component->IsEnabled())
			{
				component->TryStart();
				component->Update();
			}
		}
	};

	virtual void Draw()
	{
		for (Component* component : m_Components)
		{
			if (component != nullptr && component->IsEnabled())
			{
				component->Draw();
			}
		}
	};

	//----------------------------------------------------------
	// コンポーネント
	//----------------------------------------------------------
	template<typename T>
	T* AddComponent()
	{
		T* component = new T(this);
		component->Init();
		m_Components.push_back(component);
		return component;
	}

	template<typename T>
	T* GetComponent()
	{
		for (Component* component : m_Components)
		{
			T* result = dynamic_cast<T*>(component);
			if (result) return result;
		}
		return nullptr;
	}

	const std::list<Component*>& GetComponents() const { return m_Components; }

	virtual Vector3 GetForward()
	{
		XMMATRIX RotMatrix = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);

		Vector3 Forward;
		XMStoreFloat3((XMFLOAT3*)&Forward, RotMatrix.r[2]);
		return Forward;
	}

	virtual Vector3 GetRight()
	{
		XMMATRIX RotMatrix = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);

		Vector3 Right;
		XMStoreFloat3((XMFLOAT3*)&Right, RotMatrix.r[0]);
		return Right;
	}

	bool Destroy()
	{
		if (m_Destroy)
		{
			Uninit();
			delete this;
			return true;
		}
		else
		{
			return false;
		}
	}

	// 親から見た行列
	XMMATRIX GetLocalMatrix() const
	{
		XMMATRIX ScaleMatrix = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
		XMMATRIX RotMatrix = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
		XMMATRIX TransMatrix = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
		return ScaleMatrix * RotMatrix * TransMatrix;
	}

	// 親の行列を掛けたワールド行列
	virtual XMMATRIX GetWorldMatrix() const
	{
		if (m_Parent) return GetLocalMatrix() * m_Parent->GetWorldMatrix();
		return GetLocalMatrix();
	}

	Vector3 GetWorldPosition() const
	{
		Vector3 pos;
		XMStoreFloat3((XMFLOAT3*)&pos, GetWorldMatrix().r[3]);
		return pos;
	}
};