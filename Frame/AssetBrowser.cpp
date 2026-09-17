#include "main.h"
#include "Renderer.h"
#include "Manager.h"
#include "GameObject.h"
#include "AssetBrowser.h"
#include "EditorGUI.h"
#include "TextureManager.h"
#include "JsonUtil.h"	// Utf8ToWide / WideToUtf8
#include "ModelRenderer.h"
#include "AnimationModel.h"
#include "BillboardRenderer.h"
#include "Audio.h"
#include "ScriptTool.h"
#include "Console.h"
#include <algorithm>
#include <cctype>

std::string                      AssetBrowser::m_CurrentFolder = "asset";
std::vector<AssetBrowser::Entry> AssetBrowser::m_Entries;
bool                             AssetBrowser::m_NeedRefresh = true;
float                            AssetBrowser::m_IconSize = 72.0f;
std::string                      AssetBrowser::m_SelectedPath;
AssetBrowser::Action             AssetBrowser::m_PendingAction = AssetBrowser::Action::None;
std::string                      AssetBrowser::m_ActionPath;
char                             AssetBrowser::m_NameBuffer[128] = "";

//=============================================================
// ファイル操作
//=============================================================
static std::string RemoveInvalidChars(const char* text)
{
	std::string result;
	for (const char* p = text; *p; p++)
		if (strchr("\\/:*?\"<>|", *p) == nullptr) result += *p;
	return result;
}

static std::string FolderOf(const std::string& path)
{
	size_t slash = path.find_last_of('\\');
	return (slash == std::string::npos) ? std::string() : path.substr(0, slash);
}

static std::string ExtensionOf(const std::string& path)
{
	size_t slash = path.find_last_of('\\');
	size_t dot = path.find_last_of('.');
	return (dot == std::string::npos || (slash != std::string::npos && dot < slash)) ? std::string() : path.substr(dot);
}

void AssetBrowser::ShowInExplorer(const std::string& path)
{
	DWORD attributes = GetFileAttributesW(Utf8ToWide(path).c_str());
	if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		// ファイルはフォルダを開いて選んだ状態にする
		std::wstring parameter = L"/select,\"" + Utf8ToWide(path) + L"\"";
		ShellExecuteW(nullptr, L"open", L"explorer.exe", parameter.c_str(), nullptr, SW_SHOWNORMAL);
	}
	else
	{
		ShellExecuteW(nullptr, L"open", Utf8ToWide(path).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}
}

void AssetBrowser::StartAction(Action action, const std::string& path)
{
	m_PendingAction = action;
	m_ActionPath = path;

	switch (action)
	{
	case Action::NewFolder: strncpy_s(m_NameBuffer, "NewFolder", _TRUNCATE); break;
	case Action::NewScene:  strncpy_s(m_NameBuffer, "NewScene", _TRUNCATE); break;
	case Action::NewScript: strncpy_s(m_NameBuffer, "NewBehaviour", _TRUNCATE); break;
	case Action::Rename:    strncpy_s(m_NameBuffer, GetStem(path).c_str(), _TRUNCATE); break;
	default: break;
	}
}

void AssetBrowser::DrawCreateMenu()
{
	if (ImGui::MenuItem("フォルダ")) StartAction(Action::NewFolder, m_CurrentFolder);
	if (ImGui::MenuItem("シーン"))   StartAction(Action::NewScene, m_CurrentFolder);
	ImGui::Separator();

	// スクリプトはインクルードパスが通っている Script フォルダ（とその中）に作る
	bool inScriptFolder = (m_CurrentFolder == "Script" || m_CurrentFolder.compare(0, 7, "Script\\") == 0);
	if (ImGui::MenuItem("C++ スクリプト")) StartAction(Action::NewScript, inScriptFolder ? m_CurrentFolder : std::string("Script"));
}

//=============================================================
// 右クリックメニュー（target が nullptr なら何もないところ）
//=============================================================
void AssetBrowser::DrawContextMenu(const Entry* target)
{
	if (ImGui::BeginMenu("作成"))
	{
		DrawCreateMenu();
		ImGui::EndMenu();
	}

	if (ImGui::MenuItem("エクスプローラーで表示")) ShowInExplorer(target ? target->Path : m_CurrentFolder);

	if (ImGui::MenuItem("開く", nullptr, false, target != nullptr))
	{
		if (target->Type == AssetType::Folder)     { m_CurrentFolder = target->Path; m_NeedRefresh = true; }
		else if (target->Type == AssetType::Scene) EditorGUI::OpenScene(target->Path);
		else if (target->Type == AssetType::Script) ScriptTool::OpenInEditor(target->Path);
		else ShellExecuteW(nullptr, L"open", Utf8ToWide(target->Path).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}

	// スクリプトの削除・名前の変更はプロジェクト（vcxproj）も直す必要があるので Visual Studio で行う
	bool editable = (target != nullptr && target->Type != AssetType::Script);
	if (ImGui::MenuItem("削除", "Delete", false, editable))      StartAction(Action::Delete, target->Path);
	if (ImGui::MenuItem("名前の変更", "F2", false, editable))     StartAction(Action::Rename, target->Path);
	if (target && target->Type == AssetType::Script) ImGui::TextDisabled("  スクリプトの削除・名前の変更は Visual Studio で");
	if (ImGui::MenuItem("パスをコピー", "Alt+Ctrl+C", false, target != nullptr)) ImGui::SetClipboardText(target->Path.c_str());

	ImGui::Separator();
	if (ImGui::MenuItem("更新", "Ctrl+R")) m_NeedRefresh = true;
}

//=============================================================
// 名前入力・削除確認のダイアログ
//=============================================================
void AssetBrowser::DrawDialogs()
{
	static const char* POPUP_NAME = "AssetDialog";
	static Action openAction = Action::None;

	if (m_PendingAction != Action::None)
	{
		openAction = m_PendingAction;
		m_PendingAction = Action::None;
		ImGui::OpenPopup(POPUP_NAME);
	}

	if (!ImGui::BeginPopupModal(POPUP_NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) return;

	bool close = ImGui::IsKeyPressed(ImGuiKey_Escape);

	if (openAction == Action::Delete)
	{
		ImGui::Text("削除しますか？（ごみ箱に移動します）");
		ImGui::TextDisabled("%s", m_ActionPath.c_str());
		if (ImGui::Button("削除", ImVec2(120.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Enter))
		{
			// SHFileOperation はパスの最後に \0 が2つ必要
			std::wstring from = Utf8ToWide(m_ActionPath);
			from.push_back(L'\0');
			SHFILEOPSTRUCTW operation{};
			operation.wFunc = FO_DELETE;
			operation.pFrom = from.c_str();
			operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
			SHFileOperationW(&operation);

			if (m_SelectedPath == m_ActionPath) m_SelectedPath.clear();
			m_NeedRefresh = true;
			close = true;
		}
	}
	else
	{
		const char* title = "名前";
		if (openAction == Action::NewFolder) title = "新しいフォルダの名前";
		if (openAction == Action::NewScene)  title = "新しいシーンの名前";
		if (openAction == Action::NewScript) title = "新しいスクリプト（クラス）の名前";
		if (openAction == Action::Rename)    title = "新しい名前";
		ImGui::Text("%s", title);

		ImGui::SetNextItemWidth(280.0f);
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		bool enter = ImGui::InputText("##name", m_NameBuffer, sizeof(m_NameBuffer),
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

		std::string name = RemoveInvalidChars(m_NameBuffer);
		std::string newPath;
		if (openAction == Action::NewFolder) newPath = m_ActionPath + "\\" + name;
		if (openAction == Action::NewScene)  newPath = m_ActionPath + "\\" + name + ".json";
		if (openAction == Action::NewScript) newPath = m_ActionPath + "\\" + name + ".h / .cpp";
		if (openAction == Action::Rename)    newPath = FolderOf(m_ActionPath) + "\\" + name + ExtensionOf(m_ActionPath);

		bool exists = !name.empty() && newPath != m_ActionPath && GetFileAttributesW(Utf8ToWide(newPath).c_str()) != INVALID_FILE_ATTRIBUTES;
		if (openAction == Action::NewScript)
			exists = GetFileAttributesA((m_ActionPath + "\\" + name + ".cpp").c_str()) != INVALID_FILE_ATTRIBUTES;
		ImGui::TextDisabled("%s", newPath.c_str());
		if (exists) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "同じ名前があります");

		bool validName = (openAction != Action::NewScript) || ScriptTool::IsValidClassName(name);
		if (!validName) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "英字で始め、英数字と _ だけにしてください");
		if (openAction == Action::NewScript)
			ImGui::TextDisabled("作成後、Visual Studio でプロジェクトを再読み込みしてビルドしてください");

		bool valid = !name.empty() && !exists && validName;
		ImGui::BeginDisabled(!valid);
		if (ImGui::Button("OK", ImVec2(120.0f, 0.0f)) || (enter && valid))
		{
			std::wstring wide = Utf8ToWide(newPath);
			if (openAction == Action::NewFolder)
			{
				CreateDirectoryW(wide.c_str(), nullptr);
			}
			else if (openAction == Action::NewScene)
			{
				// カメラが1つだけある空のシーン
				FILE* file = _wfopen(wide.c_str(), L"wb");
				if (file)
				{
					fputs("{\n  \"version\": 1,\n  \"objects\": [\n    { \"id\": 1, \"class\": \"GameObject\", \"name\": \"Main Camera\", \"parent\": 0, \"layer\": 1,\n"
						"      \"position\": [0, 1, -10], \"rotation\": [0, 0, 0], \"scale\": [1, 1, 1],\n"
						"      \"components\": [ { \"type\": \"CameraComponent\", \"enabled\": true } ] }\n  ]\n}\n", file);
					fclose(file);
				}
			}
			else if (openAction == Action::NewScript)
			{
				std::string error;
				if (!ScriptTool::CreateScript(m_ActionPath, name, error)) Debug::LogError("スクリプトを作れませんでした: %s", error.c_str());
				m_CurrentFolder = m_ActionPath;	// 作った場所を表示する
			}
			else if (openAction == Action::Rename && newPath != m_ActionPath)
			{
				MoveFileW(Utf8ToWide(m_ActionPath).c_str(), wide.c_str());
				if (m_SelectedPath == m_ActionPath) m_SelectedPath = newPath;
			}
			m_NeedRefresh = true;
			close = true;
		}
		ImGui::EndDisabled();
	}

	ImGui::SameLine();
	if (ImGui::Button("キャンセル", ImVec2(120.0f, 0.0f))) close = true;

	if (close) ImGui::CloseCurrentPopup();
	ImGui::EndPopup();
}

static const char* PAYLOAD_ASSET = "ASSET_PATH";

//=============================================================
// ファイルの種類
//=============================================================
static std::string ToLower(std::string text)
{
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	return text;
}

AssetBrowser::AssetType AssetBrowser::GetType(const std::string& path)
{
	size_t dot = path.find_last_of('.');
	if (dot == std::string::npos) return AssetType::Other;
	std::string ext = ToLower(path.substr(dot + 1));

	if (ext == "obj") return AssetType::Model;
	if (ext == "fbx") return AssetType::AnimationModel;
	if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp") return AssetType::Texture;
	if (ext == "wav") return AssetType::Audio;
	if (ext == "json") return AssetType::Scene;
	if (ext == "cpp" || ext == "h" || ext == "hpp") return AssetType::Script;
	return AssetType::Other;
}

std::string AssetBrowser::GetFileName(const std::string& path)
{
	size_t slash = path.find_last_of("\\/");
	return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

std::string AssetBrowser::GetStem(const std::string& path)
{
	std::string name = GetFileName(path);
	size_t dot = name.find_last_of('.');
	return (dot == std::string::npos) ? name : name.substr(0, dot);
}


//=============================================================
// フォルダの中身を読み直す（毎フレーム読むと重いので、移動したときと更新ボタンのときだけ）
//=============================================================
void AssetBrowser::Refresh()
{
	m_Entries.clear();
	m_NeedRefresh = false;

	std::wstring pattern = Utf8ToWide(m_CurrentFolder) + L"\\*";
	WIN32_FIND_DATAW find;
	HANDLE handle = FindFirstFileW(pattern.c_str(), &find);
	if (handle == INVALID_HANDLE_VALUE) return;

	do
	{
		std::string name = WideToUtf8(find.cFileName);
		if (name == "." || name == "..") continue;
		if (find.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) continue;

		Entry entry;
		entry.Name = name;
		entry.Path = m_CurrentFolder + "\\" + name;
		entry.Type = (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? AssetType::Folder : GetType(name);
		if (entry.Type == AssetType::Other) continue;	// .mtl などは表示しない

		m_Entries.push_back(entry);
	} while (FindNextFileW(handle, &find));
	FindClose(handle);

	// フォルダを先に、あとは名前順
	std::sort(m_Entries.begin(), m_Entries.end(), [](const Entry& a, const Entry& b)
	{
		bool fa = (a.Type == AssetType::Folder), fb = (b.Type == AssetType::Folder);
		if (fa != fb) return fa;
		return ToLower(a.Name) < ToLower(b.Name);
	});
}

//=============================================================
// アイコン（テクスチャは縮小画像、それ以外は色付きの四角と種類名）
//=============================================================
void AssetBrowser::DrawIcon(const Entry& entry, const ImVec2& min, const ImVec2& max)
{
	ImDrawList* dl = ImGui::GetWindowDrawList();
	float w = max.x - min.x;

	if (entry.Type == AssetType::Texture)
	{
		ID3D11ShaderResourceView* texture = TextureManager::Load(Utf8ToWide(entry.Path).c_str());
		if (texture)
		{
			dl->AddImage((ImTextureID)(intptr_t)texture, min, max);
			return;
		}
	}

	if (entry.Type == AssetType::Folder)
	{
		// タブ付きのフォルダ
		ImU32 color = IM_COL32(222, 178, 92, 255);
		dl->AddRectFilled(ImVec2(min.x + w * 0.10f, min.y + w * 0.18f), ImVec2(min.x + w * 0.45f, min.y + w * 0.30f), color, 3.0f);
		dl->AddRectFilled(ImVec2(min.x + w * 0.10f, min.y + w * 0.26f), ImVec2(max.x - w * 0.10f, max.y - w * 0.18f), color, 4.0f);
		return;
	}

	ImU32 color = IM_COL32(110, 110, 120, 255);
	const char* label = "?";
	switch (entry.Type)
	{
	case AssetType::Model:          color = IM_COL32(70, 130, 200, 255);  label = "OBJ";   break;
	case AssetType::AnimationModel: color = IM_COL32(120, 90, 200, 255);  label = "FBX";   break;
	case AssetType::Audio:          color = IM_COL32(60, 160, 110, 255);  label = "WAV";   break;
	case AssetType::Scene:          color = IM_COL32(200, 110, 60, 255);  label = "SCENE"; break;
	case AssetType::Script:
		color = IM_COL32(40, 120, 190, 255);
		label = (entry.Path.size() >= 2 && entry.Path.substr(entry.Path.size() - 2) == ".h") ? "C++ .h" : "C++";
		break;
	default: break;
	}
	dl->AddRectFilled(ImVec2(min.x + w * 0.12f, min.y + w * 0.12f), ImVec2(max.x - w * 0.12f, max.y - w * 0.12f), color, 6.0f);
	ImVec2 textSize = ImGui::CalcTextSize(label);
	dl->AddText(ImVec2((min.x + max.x - textSize.x) * 0.5f, (min.y + max.y - textSize.y) * 0.5f), IM_COL32(255, 255, 255, 255), label);
}

//=============================================================
// Project ウィンドウ
//=============================================================
void AssetBrowser::Draw(bool* open)
{
	if (!ImGui::Begin("Project", open))
	{
		ImGui::End();
		return;
	}

	if (m_NeedRefresh) Refresh();

	// 表示するフォルダの切り替え（アセット / スクリプト）
	{
		bool inScript = (m_CurrentFolder.compare(0, 6, "Script") == 0);
		if (!inScript) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		if (ImGui::SmallButton("Assets")) { m_CurrentFolder = "asset"; m_NeedRefresh = true; }
		if (!inScript) ImGui::PopStyleColor();
		ImGui::SameLine();
		if (inScript) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		if (ImGui::SmallButton("Scripts")) { m_CurrentFolder = "Script"; m_NeedRefresh = true; }
		if (inScript) ImGui::PopStyleColor();
		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
	}

	// パンくずリスト（asset > model）。押すとそのフォルダへ戻る
	{
		std::string accumulated;
		size_t start = 0;
		int index = 0;
		while (start <= m_CurrentFolder.size())
		{
			size_t end = m_CurrentFolder.find('\\', start);
			if (end == std::string::npos) end = m_CurrentFolder.size();
			std::string part = m_CurrentFolder.substr(start, end - start);
			accumulated += (accumulated.empty() ? "" : "\\") + part;

			if (index > 0) { ImGui::SameLine(); ImGui::TextDisabled(">"); ImGui::SameLine(); }
			ImGui::PushID(index++);
			if (ImGui::SmallButton(part.c_str()))
			{
				m_CurrentFolder = accumulated;
				m_NeedRefresh = true;
			}
			ImGui::PopID();
			start = end + 1;
		}

		ImGui::SameLine(ImGui::GetWindowWidth() - 60.0f);
		if (ImGui::SmallButton("＋")) ImGui::OpenPopup("##CreateButtonMenu");
		if (ImGui::BeginPopup("##CreateButtonMenu"))
		{
			DrawCreateMenu();
			ImGui::EndPopup();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("更新")) m_NeedRefresh = true;
	}
	ImGui::Separator();

	// 並べて表示
	// Ctrl+ホイールでアイコンの大きさを変える（ホイールだけのときは今までどおりスクロール）
	ImGuiIO& io = ImGui::GetIO();
	bool resize = io.KeyCtrl && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
	if (resize && io.MouseWheel != 0.0f)
	{
		m_IconSize += io.MouseWheel * 8.0f;
		if (m_IconSize < 32.0f)  m_IconSize = 32.0f;
		if (m_IconSize > 160.0f) m_IconSize = 160.0f;
	}

	ImGui::BeginChild("##AssetGrid", ImVec2(0, 0), ImGuiChildFlags_None, resize ? ImGuiWindowFlags_NoScrollWithMouse : 0);

	// 何もないところを右クリック
	if (ImGui::BeginPopupContextWindow("##AssetContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		DrawContextMenu(nullptr);
		ImGui::EndPopup();
	}

	// ショートカット（Project にマウスがあるとき）
	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && !io.WantTextInput)
	{
		if (io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_R, false)) m_NeedRefresh = true;
		if (!m_SelectedPath.empty())
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) StartAction(Action::Delete, m_SelectedPath);
			if (ImGui::IsKeyPressed(ImGuiKey_F2, false))     StartAction(Action::Rename, m_SelectedPath);
			if (io.KeyCtrl && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_C, false)) ImGui::SetClipboardText(m_SelectedPath.c_str());
		}
	}
	float cellWidth = m_IconSize + 16.0f;
	int columns = (int)(ImGui::GetContentRegionAvail().x / cellWidth);
	if (columns < 1) columns = 1;

	std::string openFolder;
	for (int i = 0; i < (int)m_Entries.size(); i++)
	{
		const Entry& entry = m_Entries[i];
		if (i % columns != 0) ImGui::SameLine();

		ImGui::PushID(i);
		ImGui::BeginGroup();

		ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##asset", ImVec2(cellWidth, m_IconSize + ImGui::GetTextLineHeight() + 8.0f));
		bool hovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right)) m_SelectedPath = entry.Path;

		// ファイルやフォルダを右クリック
		if (ImGui::BeginPopupContextItem("##AssetItemContext"))
		{
			m_SelectedPath = entry.Path;
			DrawContextMenu(&entry);
			ImGui::EndPopup();
		}

		// ダブルクリック：フォルダなら入る、シーンなら開く
		if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if (entry.Type == AssetType::Folder) openFolder = entry.Path;
			else if (entry.Type == AssetType::Scene) EditorGUI::OpenScene(entry.Path);
			else if (entry.Type == AssetType::Script) ScriptTool::OpenInEditor(entry.Path);	// Visual Studio で開く
		}

		// ドラッグ元（フォルダ以外）
		if (entry.Type != AssetType::Folder && ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload(PAYLOAD_ASSET, entry.Path.c_str(), entry.Path.size() + 1);
			ImGui::Text("%s", entry.Name.c_str());
			ImGui::EndDragDropSource();
		}

		if (entry.Path == m_SelectedPath)
		{
			ImGui::GetWindowDrawList()->AddRectFilled(cursor, ImVec2(cursor.x + cellWidth, cursor.y + m_IconSize + ImGui::GetTextLineHeight() + 8.0f), IM_COL32(60, 110, 200, 90), 4.0f);
		}
		if (hovered)
		{
			ImGui::GetWindowDrawList()->AddRectFilled(cursor, ImGui::GetItemRectMax(), IM_COL32(255, 255, 255, 25), 4.0f);
			ImGui::SetTooltip("%s", entry.Path.c_str());
		}

		ImVec2 iconMin(cursor.x + 8.0f, cursor.y + 4.0f);
		DrawIcon(entry, iconMin, ImVec2(iconMin.x + m_IconSize, iconMin.y + m_IconSize));

		// 名前（長い名前は切る）
		std::string label = entry.Name;
		while (label.size() > 3 && ImGui::CalcTextSize(label.c_str()).x > cellWidth - 4.0f)
		{
			label = label.substr(0, label.size() - 4) + "..";
		}
		float textWidth = ImGui::CalcTextSize(label.c_str()).x;
		ImGui::GetWindowDrawList()->AddText(ImVec2(cursor.x + (cellWidth - textWidth) * 0.5f, cursor.y + m_IconSize + 6.0f),
			ImGui::GetColorU32(ImGuiCol_Text), label.c_str());

		ImGui::EndGroup();
		ImGui::PopID();
	}

	if (m_Entries.empty()) ImGui::TextDisabled("表示できるファイルがありません");

	ImGui::EndChild();

	DrawDialogs();

	ImGui::End();

	if (!openFolder.empty())
	{
		m_CurrentFolder = openFolder;
		m_NeedRefresh = true;
	}
}

//=============================================================
// ドロップ先
//=============================================================
bool AssetBrowser::AcceptDrop(AssetType type, std::string& outPath)
{
	bool accepted = false;
	if (ImGui::BeginDragDropTarget())
	{
		// 種類が合うときだけ受け取る（合わないときは枠も光らない）
		const ImGuiPayload* payload = ImGui::GetDragDropPayload();
		if (payload && payload->IsDataType(PAYLOAD_ASSET) && GetType((const char*)payload->Data) == type)
		{
			if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(PAYLOAD_ASSET))
			{
				outPath = (const char*)dropped->Data;
				accepted = true;
			}
		}
		ImGui::EndDragDropTarget();
	}
	return accepted;
}

//=============================================================
// アセットからオブジェクトを作る
//=============================================================
GameObject* AssetBrowser::CreateObject(const std::string& path, const Vector3& position)
{
	AssetType type = GetType(path);
	if (type == AssetType::Folder || type == AssetType::Scene || type == AssetType::Script || type == AssetType::Other) return nullptr;

	GameObject* object = Manager::CreateGameObject(GetStem(path));
	object->SetPosition(position);

	switch (type)
	{
	case AssetType::Model:
		object->AddComponent<ModelRenderer>()->Load(path.c_str());
		break;

	case AssetType::AnimationModel:
		object->AddComponent<AnimationModel>()->Load(path.c_str());
		object->SetScale({ 0.01f, 0.01f, 0.01f });	// FBX はセンチメートル単位のことが多い
		break;

	case AssetType::Texture:
	{
		BillboardRenderer* renderer = object->AddComponent<BillboardRenderer>();
		renderer->Load(Utf8ToWide(path).c_str());
		renderer->SetMode(BillboardMode::AxisY);
		renderer->SetAnchorBottom(true);
		renderer->SetSize(2.0f, 2.0f);
		object->SetLayer(2);
		break;
	}

	case AssetType::Audio:
		object->AddComponent<Audio>()->Load(path.c_str());
		break;

	default:
		break;
	}
	return object;
}
