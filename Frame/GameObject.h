#pragma once

#include "main.h"
#include "Vector3.h"
#include "Component.h"

class GameObject
{
protected://外部からアクセスできないが、継承したクラスからアクセスできる
	Vector3 m_Position{ 0.0f, 0.0f, 0.0f };
	Vector3 m_Rotation{ 0.0f, 0.0f, 0.0f };
	Vector3 m_Scale{ 1.0f, 1.0f, 1.0f };
	bool m_Destroy = false;
	int m_Layer = 1;	// レイヤー番号
	float m_CameraZ;	//ソート用Z値

	ID3D11Buffer*							m_vertexBuffer=nullptr;					// 頂点バッファ
	ID3D11InputLayout*					m_VertexLayout=nullptr;				// 頂点レイアウト
	ID3D11VertexShader*				m_VertexShader=nullptr;				// 頂点シェーダー
	ID3D11PixelShader*					m_PixelShader=nullptr;					// ピクセルシェーダー	
	ID3D11Buffer*							m_indexBuffer=nullptr;					// インデックスバッファ
	ID3D11ShaderResourceView*  m_Texture=nullptr;							// テクスチャ

	std::list<Component*> m_Components;

public:
	void SetPosition(const Vector3& position) { m_Position = position; }
	Vector3 GetPosition() const { return m_Position; }
	void SetRotation(const Vector3& rotation) { m_Rotation = rotation; }
	Vector3 GetRotation() const { return m_Rotation; }
	void SetScale(const Vector3& scale) { m_Scale = scale; }
	Vector3 GetScale() const { return m_Scale; }
	void SetDestroy() { m_Destroy = true; }
	bool IsDestroyed() const { return m_Destroy; }	
	virtual void OnCollision(GameObject* other) {}
	virtual void OnPushed(const Vector3& push) {}
	int GetLayer() { return m_Layer ; }
	float GetCameraZ() const { return m_CameraZ; }
	void CalcCameraZ(Vector3 CameraPos, Vector3 CameraForward)
	{
		Vector3 dir = m_Position - CameraPos;
		m_CameraZ = Vector3::dot(dir, CameraForward);	//内積
	}

	virtual ~GameObject() {}
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
			if (component != nullptr)
			{
				component->Update();
			}
		}
	};

	virtual void Draw()
	{
		for (Component* component : m_Components)
		{
			if (component != nullptr)
			{
				component->Draw();
			}
		}
	};

	template<typename T>
    T* AddComponent(GameObject* gameObject)
	{
		T* component = new T(gameObject);
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

		Vector3 Forward;
		XMStoreFloat3((XMFLOAT3*)&Forward, RotMatrix.r[0]);
		return Forward;

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

	virtual XMMATRIX GetWorldMatrix() const
	{
		XMMATRIX ScaleMatrix = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
		XMMATRIX RotMatrix = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
		XMMATRIX TransMatrix = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
		return ScaleMatrix * RotMatrix * TransMatrix;
	}
};