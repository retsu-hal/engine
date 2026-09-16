#pragma once


#include "main.h"
#include "Profiler.h"
#include <typeinfo>
#include <string>
#include "GameObject.h"

// 前方宣言
class Scene;

class Manager
{

private:
	static std::list<GameObject*> m_GameObjects;
	
	static float m_DeltaTime;
	static Scene* m_Scene;
	static Scene* m_NextScene;
	static float m_ChangeSceneTime;

public:
	static void Init();
	static void Uninit();
	static void Update();
	static void Draw();
	static float GetDeltaTime() { return m_DeltaTime; }
	static void SetDeltaTime(float dt) { m_DeltaTime = dt; }

	static const std::list<GameObject*>& GetAllGameObjects() { return m_GameObjects; }
	static GameObject* FindGameObjectByID(unsigned int id);

	template<typename T>
	static T* AddGameObject()
	{
		T* gameObject = nullptr;
		{
			gameObject = new T();
			gameObject->SetName(TypeName(typeid(T).name()));	
			gameObject->Init();
		}
		m_GameObjects.push_back(gameObject);

		return gameObject;
	}

	// GameObject の完全な型が必要なので定義は cpp 側に置く
	static void RemoveGameObject(GameObject* gameobject);

	// 継承クラスを作らず、空の GameObject にコンポーネントを付けて使う
	static GameObject* CreateGameObject(const std::string& name);

	// T 型のコンポーネントを持つ最初のものを探す（例: FindComponent<PlayerController>()）
	template<typename T>
	static T* FindComponent()
	{
		for (GameObject* gameObject : m_GameObjects)
		{
			if (gameObject->IsDestroyed()) continue;
			T* component = gameObject->GetComponent<T>();
			if (component != nullptr) return component;
		}
		return nullptr;
	}

	// T 型のコンポーネントをすべて集める
	template<typename T>
	static std::vector<T*> FindComponents()
	{
		std::vector<T*> components;
		for (GameObject* gameObject : m_GameObjects)
		{
			if (gameObject->IsDestroyed()) continue;
			T* component = gameObject->GetComponent<T>();
			if (component != nullptr) components.push_back(component);
		}
		return components;
	}

	template<typename T>
	static T* GetGameObject()
	{
		for (GameObject* gameObject : m_GameObjects)
		{
			T* find = dynamic_cast<T*>(gameObject);
			if (find!=nullptr)
			{
				return find;
			}
		}

		return nullptr;
	}

	template<typename T>
	static std::vector<T*>GetGameObjects()
	{
		std::vector<T*> gameObjects;
		for (GameObject* gameObject : m_GameObjects)
		{
			T* find = dynamic_cast<T*>(gameObject);
			if (find != nullptr)
			{
				gameObjects.push_back(find);
			}
		}
		return gameObjects;
	}

	template<typename T>
	static void ChangeScene(float Time=0.0f)
	{
		if(m_NextScene == nullptr)
		{
			m_ChangeSceneTime = Time;
			m_NextScene = new T();
		}
	}
};