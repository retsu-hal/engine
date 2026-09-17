#include "main.h"
#include "Manager.h"
#include "GameObject.h"
#include "SceneSerializer.h"
#include "Registry.h"
#include "JsonUtil.h"
#include "Profiler.h"	// TypeName
#include "Console.h"
#include <typeinfo>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>

static void Log(const std::string& text)
{
	// 失敗を表す内容は警告として出す
	bool warning = text.find("できない") != std::string::npos || text.find("失敗") != std::string::npos
		|| text.find("見つかりません") != std::string::npos || text.find("開けません") != std::string::npos;
	if (warning) Console::Add(LogLevel::Warning, "[Scene] " + text);
	else         Console::Add(LogLevel::Info, "[Scene] " + text);
}

//=============================================================
// 1つのオブジェクトを JSON にする（作り直せないクラスなら false）
//=============================================================
static bool SerializeObject(GameObject* object, json& data)
{
	std::string className = TypeName(typeid(*object).name());
	bool isPlain = (className == "GameObject");

	// 登録されていない継承クラス（Polygon2D など）は作り直せないので保存しない
	if (!isPlain && GameObjectRegistry::GetAll().count(className) == 0)
	{
		Log("保存できないオブジェクト: " + object->GetName() + " (" + className + ")");
		return false;
	}

	data["id"] = object->GetID();
	data["class"] = className;
	data["name"] = object->GetName();
	data["parent"] = object->GetParent() ? object->GetParent()->GetID() : 0;
	data["layer"] = object->GetLayer();
	data["position"] = ToJson(object->GetPosition());
	data["rotation"] = ToJson(object->GetRotation());
	data["scale"] = ToJson(object->GetScale());

	// 継承クラスは Init でコンポーネントを付けるので、保存するのは空の GameObject のものだけ
	if (isPlain)
	{
		json& components = data["components"];
		components = json::array();
		for (Component* component : object->GetComponents())
		{
			std::string typeName = TypeName(typeid(*component).name());
			if (ComponentRegistry::GetAll().count(typeName) == 0)
			{
				Log("保存できないコンポーネント: " + object->GetName() + " / " + typeName);
				continue;
			}

			json componentData;
			componentData["type"] = typeName;
			componentData["enabled"] = component->IsEnabled();
			component->Serialize(componentData);
			components.push_back(componentData);
		}
	}
	return true;
}

//=============================================================
// JSON の objects 配列からオブジェクトを作る（作ったものを created に入れる）
//=============================================================
static void CreateObjects(const json& objects, std::vector<GameObject*>& created)
{
	std::map<unsigned int, GameObject*> idMap;			// ファイルの ID → 作ったオブジェクト
	std::map<GameObject*, unsigned int> parentMap;		// 作ったオブジェクト → 親のファイル上の ID

	// 1回目：オブジェクトとコンポーネントを作る
	for (const json& data : objects)
	{
		std::string className = data.value("class", "GameObject");
		std::string name = data.value("name", className);

		GameObject* object = nullptr;
		if (className == "GameObject") object = Manager::CreateGameObject(name);
		else object = Manager::AddGameObjectInstance(GameObjectRegistry::Create(className), name);

		if (object == nullptr)
		{
			Log("作れないクラス: " + className);
			continue;
		}

		Vector3 position = object->GetPosition();
		Vector3 rotation = object->GetRotation();
		Vector3 scale = object->GetScale();
		int layer = object->GetLayer();
		JsonRead(data, "position", position);
		JsonRead(data, "rotation", rotation);
		JsonRead(data, "scale", scale);
		JsonRead(data, "layer", layer);
		object->SetPosition(position);
		object->SetRotation(rotation);
		object->SetScale(scale);
		object->SetLayer(layer);

		if (data.contains("components"))
		{
			for (const json& componentData : data["components"])
			{
				std::string type = componentData.value("type", "");
				Component* component = object->AddComponentInstance(ComponentRegistry::Create(type, object));
				if (component == nullptr)
				{
					Log("作れないコンポーネント: " + type);
					continue;
				}
				component->SetEnabled(componentData.value("enabled", true));
				component->Deserialize(componentData);
			}
		}

		idMap[data.value("id", 0u)] = object;
		unsigned int parentID = data.value("parent", 0u);
		if (parentID != 0) parentMap[object] = parentID;
		created.push_back(object);
	}

	// 2回目：親子関係をつなぐ（親が後に書かれていても大丈夫なように分ける）
	for (auto& pair : parentMap)
	{
		auto it = idMap.find(pair.second);
		if (it != idMap.end()) pair.first->SetParent(it->second);
	}
}

//=============================================================
// 保存
//=============================================================
int SceneSerializer::SaveToText(std::string& outText)
{
	json root;
	root["version"] = 1;
	json& objects = root["objects"];
	objects = json::array();

	int skipped = 0;
	for (GameObject* object : Manager::GetAllGameObjects())
	{
		if (object->IsDestroyed()) continue;

		json data;
		if (SerializeObject(object, data)) objects.push_back(data);
		else skipped++;
	}

	// 日本語の名前が壊れていても保存が止まらないように replace を指定
	outText = root.dump(2, ' ', false, json::error_handler_t::replace);
	return skipped;
}

int SceneSerializer::SaveToFile(const std::string& path)
{
	std::string text;
	int skipped = SaveToText(text);

	std::ofstream file(path, std::ios::binary);
	if (!file)
	{
		Log("ファイルを開けません: " + path);
		return -1;
	}
	file << text;
	Log("保存しました: " + path);
	return skipped;
}

//=============================================================
// 読み込み
//=============================================================
bool SceneSerializer::LoadFromText(const std::string& text)
{
	json root;
	try
	{
		root = json::parse(text);
	}
	catch (const std::exception& e)
	{
		Log(std::string("JSON の読み込みに失敗: ") + e.what());
		return false;
	}

	if (!root.contains("objects") || !root["objects"].is_array()) return false;

	std::vector<GameObject*> created;
	CreateObjects(root["objects"], created);
	return true;
}

bool SceneSerializer::LoadFromFile(const std::string& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		Log("ファイルが見つかりません: " + path);
		return false;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	Log("読み込みます: " + path);
	return LoadFromText(buffer.str());
}

//=============================================================
// 複製（保存と読み込みの仕組みをそのまま使う）
//=============================================================
static void CollectTree(GameObject* object, std::vector<GameObject*>& out)
{
	if (object->IsDestroyed()) return;
	out.push_back(object);
	for (GameObject* child : object->GetChildren()) CollectTree(child, out);
}

GameObject* SceneSerializer::Duplicate(GameObject* source)
{
	if (source == nullptr) return nullptr;

	std::vector<GameObject*> tree;
	CollectTree(source, tree);

	json objects = json::array();
	for (GameObject* object : tree)
	{
		json data;
		if (!SerializeObject(object, data)) continue;

		// 一番上は元の親に付ける（親の ID は複製の中にないので、つながずに後で付け直す）
		if (object == source) data["parent"] = 0;
		objects.push_back(data);
	}
	if (objects.empty()) return nullptr;

	std::vector<GameObject*> created;
	CreateObjects(objects, created);
	if (created.empty()) return nullptr;

	GameObject* root = created[0];
	root->SetParent(source->GetParent());
	return root;
}
