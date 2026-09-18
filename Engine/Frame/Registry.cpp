#include "Registry.h"

// 関数内の static にしておくと、他の cpp の登録より先に必ず作られる
std::map<std::string, ComponentRegistry::Factory>& ComponentRegistry::GetAll()
{
	static std::map<std::string, Factory> factories;
	return factories;
}

std::map<std::string, GameObjectRegistry::Factory>& GameObjectRegistry::GetAll()
{
	static std::map<std::string, Factory> factories;
	return factories;
}

std::map<std::string, SceneRegistry::Factory>& SceneRegistry::GetAll()
{
	static std::map<std::string, Factory> factories;
	return factories;
}

// GameScripts.dll を外す前に呼ぶ
// 登録されている「作る関数」は DLL の中のコードを指しているので、外した後に触ると落ちる
void ClearAllRegistries()
{
	ComponentRegistry::GetAll().clear();
	GameObjectRegistry::GetAll().clear();
	SceneRegistry::GetAll().clear();
}
