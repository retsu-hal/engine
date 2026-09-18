#pragma once
#include "EngineAPI.h"
#include <string>
#include <map>
#include <functional>

class GameObject;
class Component;

//=============================================================
// 名前（文字列）からコンポーネントやオブジェクトを作るための登録表
// シーンファイルの読み込みと、Inspector の「Add Component」で使う
//=============================================================
class ENGINE_API ComponentRegistry
{
public:
	using Factory = std::function<Component*(GameObject*)>;

	// 登録表は Engine.dll の中に1つだけ（GameScripts.dll から登録しても同じ表に入る）
	// ※ ヘッダーの関数内 static にすると DLL ごとに別の表になってしまうので cpp に置く
	static std::map<std::string, Factory>& GetAll();

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
class ENGINE_API GameObjectRegistry
{
public:
	using Factory = std::function<GameObject*()>;

	static std::map<std::string, Factory>& GetAll();

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

// シーンのクラス（TitleScene など）用。ProjectSettings の StartScene に名前を書くと最初に作られる
class Scene;
class ENGINE_API SceneRegistry
{
public:
	using Factory = std::function<Scene*()>;

	static std::map<std::string, Factory>& GetAll();

	static bool Register(const std::string& name, Factory factory)
	{
		GetAll()[name] = factory;
		return true;
	}

	static Scene* Create(const std::string& name)
	{
		auto it = GetAll().find(name);
		return (it != GetAll().end()) ? it->second() : nullptr;
	}
};

// DLL を外す前に、登録表を空にする（登録されている関数は DLL の中にあるため）
ENGINE_API void ClearAllRegistries();

// cpp の最後に書くと、そのクラスが名前で作れるようになる
// 例: REGISTER_COMPONENT(Rigidbody)
#define REGISTER_COMPONENT(T) \
	static bool s_RegisteredComponent_##T = ComponentRegistry::Register(#T, [](GameObject* owner) -> Component* { return new T(owner); });

#define REGISTER_GAMEOBJECT(T) \
	static bool s_RegisteredGameObject_##T = GameObjectRegistry::Register(#T, []() -> GameObject* { return new T(); });

#define REGISTER_SCENE(T) \
	static bool s_RegisteredScene_##T = SceneRegistry::Register(#T, []() -> Scene* { return new T(); });
