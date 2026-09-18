#include "main.h"
#include "ProjectSettings.h"
#include "JsonUtil.h"
#include <fstream>
#include <sstream>

std::string ProjectSettings::Title = "GM31 Game";
std::string ProjectSettings::StartScene = "TitleScene";	// .json のパス、または REGISTER_SCENE したクラス名

std::vector<std::string> ProjectSettings::Tags = { "Untagged", "Respawn", "Finish", "EditorOnly", "MainCamera", "Player", "Enemy" };

static const char* SETTINGS_PATH = "asset\\project.json";

void ProjectSettings::Load()
{
	std::ifstream file(SETTINGS_PATH, std::ios::binary);
	if (!file) return;

	std::stringstream buffer;
	buffer << file.rdbuf();
	try
	{
		json data = json::parse(buffer.str());
		JsonRead(data, "title", Title);
		JsonRead(data, "startScene", StartScene);
		std::vector<std::string> tags;
		JsonRead(data, "tags", tags);
		for (const std::string& tag : tags) AddTag(tag);
	}
	catch (...)
	{
	}
}

void ProjectSettings::Save()
{
	json data;
	data["title"] = Title;
	data["startScene"] = StartScene;
	data["tags"] = Tags;
	std::ofstream(SETTINGS_PATH, std::ios::binary) << data.dump(2, ' ', false, json::error_handler_t::replace);
}

// 同じ名前がなければ追加する
void ProjectSettings::AddTag(const std::string& tag)
{
	if (tag.empty()) return;
	for (const std::string& t : Tags) if (t == tag) return;
	Tags.push_back(tag);
}
