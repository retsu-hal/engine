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
#include <algorithm>
#include <cctype>

std::string                      AssetBrowser::m_CurrentFolder = "asset";
std::vector<AssetBrowser::Entry> AssetBrowser::m_Entries;
bool                             AssetBrowser::m_NeedRefresh = true;
float                            AssetBrowser::m_IconSize = 72.0f;
char                             AssetBrowser::m_NewFolderName[128] = "";
bool                             AssetBrowser::m_OpenNewFolderPopup = false;

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
	return AssetType::Other;
}

std::string AssetBrowser::GetStem(const std::string& path)
{
	size_t slash = path.find_last_of("\\/");
	std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
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
	default: break;
	}
	dl->AddRectFilled(ImVec2(min.x + w * 0.12f, min.y + w * 0.12f), ImVec2(max.x - w * 0.12f, max.y - w * 0.12f), color, 6.0f);
	ImVec2 textSize = ImGui::CalcTextSize(label);
	dl->AddText(ImVec2((min.x + max.x - textSize.x) * 0.5f, (min.y + max.y - textSize.y) * 0.5f), IM_COL32(255, 255, 255, 255), label);
}

//=============================================================
// Project ウィンドウ
//=============================================================
void AssetBrowser::Draw()
{
	if (!ImGui::Begin("Project"))
	{
		ImGui::End();
		return;
	}

	if (m_NeedRefresh) Refresh();

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

		ImGui::SameLine(ImGui::GetWindowWidth() - 150.0f);
		if (ImGui::SmallButton("＋フォルダ"))
		{
			strncpy_s(m_NewFolderName, "NewFolder", _TRUNCATE);
			m_OpenNewFolderPopup = true;
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

	// 何もないところを右クリック：フォルダを作る
	if (ImGui::BeginPopupContextWindow("##AssetContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("フォルダを作成"))
		{
			strncpy_s(m_NewFolderName, "NewFolder", _TRUNCATE);
			m_OpenNewFolderPopup = true;
		}
		if (ImGui::MenuItem("エクスプローラーで開く"))
		{
			ShellExecuteW(nullptr, L"open", Utf8ToWide(m_CurrentFolder).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		}
		ImGui::EndPopup();
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

		// ダブルクリック：フォルダなら入る、シーンなら開く
		if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if (entry.Type == AssetType::Folder) openFolder = entry.Path;
			else if (entry.Type == AssetType::Scene) EditorGUI::OpenScene(entry.Path);
		}

		// ドラッグ元（フォルダ以外）
		if (entry.Type != AssetType::Folder && ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload(PAYLOAD_ASSET, entry.Path.c_str(), entry.Path.size() + 1);
			ImGui::Text("%s", entry.Name.c_str());
			ImGui::EndDragDropSource();
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

	// フォルダ名の入力
	if (m_OpenNewFolderPopup)
	{
		ImGui::OpenPopup("NewFolder");
		m_OpenNewFolderPopup = false;
	}
	if (ImGui::BeginPopupModal("NewFolder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("%s に作るフォルダの名前", m_CurrentFolder.c_str());
		ImGui::SetNextItemWidth(260.0f);
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		bool enter = ImGui::InputText("##folder", m_NewFolderName, sizeof(m_NewFolderName),
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

		std::string name;
		for (const char* p = m_NewFolderName; *p; p++)
			if (strchr("\\/:*?\"<>|.", *p) == nullptr) name += *p;

		std::wstring path = Utf8ToWide(m_CurrentFolder + "\\" + name);
		bool exists = !name.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
		if (exists) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "同じ名前があります");

		ImGui::BeginDisabled(name.empty() || exists);
		if (ImGui::Button("作成", ImVec2(120.0f, 0.0f)) || (enter && !name.empty() && !exists))
		{
			CreateDirectoryW(path.c_str(), nullptr);
			m_NeedRefresh = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("キャンセル", ImVec2(120.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

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
	if (type == AssetType::Folder || type == AssetType::Scene || type == AssetType::Other) return nullptr;

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
