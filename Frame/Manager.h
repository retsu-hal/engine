#pragma once


#include "main.h"
#include "Profiler.h"
#include <typeinfo>

// 前方宣言
class GameObject;
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