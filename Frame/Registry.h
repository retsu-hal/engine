#pragma once
#include <string>
#include <map>
#include <functional>

class GameObject;
class Component;

//=============================================================
// 名前（文字列）からコンポーネントやオブジェクトを作るための登録表
// シーンファイルの読み込みと、Inspector の「Add Component」で使う
//=============================================================
class ComponentRegistry
{
public:
	using Factory = std::function<Component*(GameObject*)>;

	// 関数内の static にしておくと、他の cpp の登録より先に必ず作られる
	static std::map<std::string, Factory>& GetAll()
	{
		static std::map<std::string, Factory> factories;
		return factories;
	}

	static bool Register(const std::string& name, Factory factory)
	{
		GetAll()[name] = factory;
		return true;
	}

	static Component* Create(const std::string& name, GameObject* owner)
	{
		auto it = GetAll().find(name);
		return (it != GetAll().end()) ? it->second(owner) : nullptr;
	}
};

// 継承クラスのままのオブジェクト（CAMERA / Sky など）用
class GameObjectRegistry
{
public:
	using Factory = std::function<GameObject*()>;

	static std::map<std::string, Factory>& GetAll()
	{
		static std::map<std::string, Factory> factories;
		return factories;
	}

	static bool Register(const std::string& name, Factory factory)
	{
		GetAll()[name] = factory;
		return true;
	}

	static GameObject* Create(const std::string& name)
	{
		auto it = GetAll().find(name);
		return (it != GetAll().end()) ? it->second() : nullptr;
	}
};

// cpp の最後に書くと、そのクラスが名前で作れるようになる
// 例: REGISTER_COMPONENT(Rigidbody)
#define REGISTER_COMPONENT(T) \
	static bool s_RegisteredComponent_##T = ComponentRegistry::Register(#T, [](GameObject* owner) -> Component* { return new T(owner); });

#define REGISTER_GAMEOBJECT(T) \
	static bool s_RegisteredGameObject_##T = GameObjectRegistry::Register(#T, []() -> GameObject* { return new T(); });
