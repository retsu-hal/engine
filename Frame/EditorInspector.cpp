#include "main.h"
#include "EditorGUI.h"
#include "EditorUI.h"
#include "Manager.h"
#include "GameObject.h"
#include "Profiler.h"	// TypeName
#include "EditorCamera.h"
#include "Registry.h"
#include "AssetBrowser.h"
#include "UndoSystem.h"
#include "ScriptTool.h"
#include "ProjectSettings.h"
#include "Console.h"
#include "JsonUtil.h"
#include <typeinfo>
#include <cctype>

//=============================================================
// Inspector
//=============================================================
void EditorGUI::DrawInspector()
{
	ImGui::Begin("Inspector", &m_ShowInspector);

	GameObject* object = Manager::FindGameObjectByID(m_SelectedID);
	if (object == nullptr)
	{
		ImGui::TextDisabled("Hierarchy でオブジェクトを選択してください");
		if (ImGui::CollapsingHeader("Editor Camera"))
		{
			EditorCamera::OnInspectorGUI();
		}
		ImGui::End();
		return;
	}

	const float labelWidth = 90.0f;	// Transform の左の見出しの幅

	//---------------------------------------------------------
	// ヘッダー：[アクティブ] [名前] [Static]
	//---------------------------------------------------------
	bool active = object->IsActiveSelf();
	if (ImGui::Checkbox("##Active", &active)) object->SetActive(active);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("アクティブ（外すと Update / Draw / 当たり判定が止まる。子も止まる）");

	ImGui::SameLine();
	char name[128];
	strncpy_s(name, object->GetName().c_str(), _TRUNCATE);
	float staticWidth = ImGui::CalcTextSize("Static").x + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x;
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - staticWidth - ImGui::GetStyle().ItemSpacing.x);
	if (ImGui::InputText("##Name", name, sizeof(name))) object->SetName(name);

	ImGui::SameLine();
	bool isStatic = object->IsStatic();
	if (ImGui::Checkbox("Static", &isStatic)) object->SetStatic(isStatic);

	//---------------------------------------------------------
	// Tag / Layer
	//---------------------------------------------------------
	float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
	float comboLabel = ImGui::CalcTextSize("Layer").x + ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Tag");
	ImGui::SameLine(comboLabel + ImGui::GetStyle().WindowPadding.x);
	ImGui::SetNextItemWidth(half - comboLabel);
	bool openAddTag = false;
	if (ImGui::BeginCombo("##Tag", object->GetTag().c_str()))
	{
		for (const std::string& tag : ProjectSettings::Tags)
		{
			if (ImGui::Selectable(tag.c_str(), tag == object->GetTag())) object->SetTag(tag);
		}
		ImGui::Separator();
		if (ImGui::Selectable("Add Tag...")) openAddTag = true;
		ImGui::EndCombo();
	}
	if (openAddTag) ImGui::OpenPopup("AddTagPopup");
	if (ImGui::BeginPopup("AddTagPopup"))
	{
		static char newTag[64] = "";
		if (ImGui::IsWindowAppearing())
		{
			newTag[0] = '\0';
			ImGui::SetKeyboardFocusHere();
		}
		ImGui::SetNextItemWidth(180.0f);
		bool enter = ImGui::InputTextWithHint("##NewTag", "新しいタグ名", newTag, sizeof(newTag), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		if ((ImGui::Button("追加") || enter) && newTag[0] != '\0')
		{
			ProjectSettings::AddTag(newTag);
			ProjectSettings::Save();
			object->SetTag(newTag);
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	ImGui::TextUnformatted("Layer");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-1.0f);
	static const char* LAYER_NAMES[] = { "0: Camera", "1: Default", "2: Transparent", "3: UI (2D)" };
	int layer = object->GetLayer();
	if (layer < 0 || layer > 3) layer = 1;
	if (ImGui::Combo("##Layer", &layer, LAYER_NAMES, IM_ARRAYSIZE(LAYER_NAMES))) object->SetLayer(layer);

	// 親子・ID は小さく
	GameObject* parent = object->GetParent();
	ImGui::TextDisabled("ID: %u   Class: %s   Parent: %s", object->GetID(),
		TypeName(typeid(*object).name()).c_str(), parent ? parent->GetName().c_str() : "-");
	if (parent)
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("親子関係を解除")) object->SetParent(nullptr);
	}
	if (!object->IsActiveInHierarchy() && object->IsActiveSelf())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "親が非アクティブなので止まっています");
	}

	ImGui::Spacing();

	//---------------------------------------------------------
	// Transform
	//---------------------------------------------------------
	bool transformOpen = ImGui::CollapsingHeader("##TransformHeader", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
	if (ImGui::BeginPopupContextItem("TransformMenu"))
	{
		if (ImGui::MenuItem("Reset"))
		{
			object->SetPosition({ 0.0f, 0.0f, 0.0f });
			object->SetRotation({ 0.0f, 0.0f, 0.0f });
			object->SetScale({ 1.0f, 1.0f, 1.0f });
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Reset Position")) object->SetPosition({ 0.0f, 0.0f, 0.0f });
		if (ImGui::MenuItem("Reset Rotation")) object->SetRotation({ 0.0f, 0.0f, 0.0f });
		if (ImGui::MenuItem("Reset Scale"))    object->SetScale({ 1.0f, 1.0f, 1.0f });
		ImGui::EndPopup();
	}
	EditorUI::ComponentHeaderLabel("Transform", "T", ImVec4(0.55f, 0.55f, 0.6f, 1.0f), nullptr);
	if (ImGui::SmallButton("...##Transform")) ImGui::OpenPopup("TransformMenu");

	if (transformOpen)
	{
		Vector3 pos = object->GetPosition();
		if (EditorUI::Vector3Field("Position", pos, 0.05f, labelWidth)) object->SetPosition(pos);

		// 内部はラジアン、表示は度
		Vector3 rot = object->GetRotation();
		Vector3 deg(XMConvertToDegrees(rot.x), XMConvertToDegrees(rot.y), XMConvertToDegrees(rot.z));
		if (EditorUI::Vector3Field("Rotation", deg, 0.5f, labelWidth))
		{
			object->SetRotation({ XMConvertToRadians(deg.x), XMConvertToRadians(deg.y), XMConvertToRadians(deg.z) });
		}

		Vector3 scale = object->GetScale();
		if (EditorUI::Vector3Field("Scale", scale, 0.01f, labelWidth)) object->SetScale(scale);
		ImGui::Spacing();
	}

	//---------------------------------------------------------
	// コンポーネント
	//---------------------------------------------------------
	int index = 0;
	Component* removeComponent = nullptr;	// 回している最中に消すと壊れるので、ループの後で消す
	Component* moveUp = nullptr;
	Component* moveDown = nullptr;
	const std::list<Component*>& components = object->GetComponents();
	for (Component* component : components)
	{
		ImGui::PushID(index);
		std::string typeName = TypeName(typeid(*component).name());
		std::string sourceFile = ScriptTool::FindSourceFile(typeName);

		bool open = ImGui::CollapsingHeader("##ComponentHeader", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

		// 見出しをダブルクリック：スクリプトを Visual Studio で開く
		if (!sourceFile.empty() && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			ScriptTool::OpenInEditor(sourceFile);
		}

		if (ImGui::BeginPopupContextItem("ComponentMenu"))
		{
			if (ImGui::MenuItem("Reset"))
			{
				// 作り直した直後の値を読み込む
				Component* fresh = ComponentRegistry::Create(typeName, object);
				if (fresh)
				{
					nlohmann::json data;
					fresh->Serialize(data);
					delete fresh;
					component->Deserialize(data);
				}
			}
			ImGui::Separator();
			if (ImGui::MenuItem("上へ移動", nullptr, false, index > 0)) moveUp = component;
			if (ImGui::MenuItem("下へ移動", nullptr, false, index + 1 < (int)components.size())) moveDown = component;
			ImGui::Separator();
			if (ImGui::MenuItem("スクリプトを編集", nullptr, false, !sourceFile.empty())) ScriptTool::OpenInEditor(sourceFile);
			ImGui::Separator();
			if (ImGui::MenuItem("コンポーネントを削除")) removeComponent = component;
			ImGui::EndPopup();
		}

		const char* icon = "#";
		ImVec4 iconColor(0.45f, 0.75f, 0.45f, 1.0f);	// スクリプト＝緑
		EditorUI::GetComponentIcon(typeName, icon, iconColor);

		bool enabled = component->IsEnabled();
		EditorUI::ComponentHeaderLabel(EditorUI::NicifyName(typeName).c_str(), icon, iconColor, &enabled);
		if (enabled != component->IsEnabled()) component->SetEnabled(enabled);
		if (ImGui::SmallButton("...")) ImGui::OpenPopup("ComponentMenu");

		if (open)
		{
			// 無効なコンポーネントは中身を少し薄く表示する
			if (!enabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.6f);
			ImGui::Indent(6.0f);
			component->OnInspectorGUI();
			ImGui::Unindent(6.0f);
			if (!enabled) ImGui::PopStyleVar();
			ImGui::Spacing();
		}
		ImGui::PopID();
		index++;
	}

	if (removeComponent) object->RemoveComponent(removeComponent);
	if (moveUp)   object->MoveComponent(moveUp, -1);
	if (moveDown) object->MoveComponent(moveDown, +1);

	// コンポーネントを追加（登録されているものを一覧から選ぶ）
	ImGui::Separator();
	ImGui::Spacing();
	const float addWidth = 230.0f;
	float addX = (ImGui::GetContentRegionAvail().x - addWidth) * 0.5f;
	if (addX > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + addX);
	if (ImGui::Button("Add Component", ImVec2(addWidth, 0.0f))) ImGui::OpenPopup("AddComponentPopup");

	// Project の Scripts からスクリプトをドロップしても追加できる
	std::string droppedScript;
	if (AssetBrowser::AcceptDrop(AssetBrowser::AssetType::Script, droppedScript))
	{
		std::string typeName = AssetBrowser::GetStem(droppedScript);
		Component* component = ComponentRegistry::Create(typeName, object);
		if (component)
		{
			UndoSystem::Begin();
			object->AddComponentInstance(component);
		}
		else
		{
			Debug::LogWarning("%s はまだ登録されていません。ビルドし直してから追加してください", typeName.c_str());
		}
	}
	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		// 検索欄（開いたらすぐ入力できるようにする）
		static char search[64] = "";
		if (ImGui::IsWindowAppearing())
		{
			search[0] = '\0';
			ImGui::SetKeyboardFocusHere();
		}
		ImGui::SetNextItemWidth(260.0f);
		bool enter = ImGui::InputTextWithHint("##ComponentSearch", "検索", search, sizeof(search), ImGuiInputTextFlags_EnterReturnsTrue);

		// 大文字・小文字を区別せずに、名前の一部が合うものを出す
		auto lower = [](std::string s) { for (char& c : s) c = (char)tolower((unsigned char)c); return s; };
		std::string keyword = lower(search);

		ImGui::Separator();
		ImGui::BeginChild("##ComponentList", ImVec2(260.0f, 240.0f), ImGuiChildFlags_None);

		const ComponentRegistry::Factory* firstMatch = nullptr;
		int matchCount = 0;
		for (auto& pair : ComponentRegistry::GetAll())
		{
			std::string name = lower(pair.first);
			if (!keyword.empty() && name.find(keyword) == std::string::npos) continue;

			if (firstMatch == nullptr) firstMatch = &pair.second;
			matchCount++;

			// 一番上の候補は Enter で追加されるので、選択中の色にしておく
			bool isFirst = (matchCount == 1 && !keyword.empty());
			if (ImGui::Selectable(pair.first.c_str(), isFirst))
			{
				UndoSystem::Begin();
				object->AddComponentInstance(pair.second(object));
				ImGui::CloseCurrentPopup();
			}
		}

		if (matchCount == 0) ImGui::TextDisabled("見つかりません");
		ImGui::EndChild();

		// Enter：一番上の候補を追加
		if (enter && firstMatch != nullptr)
		{
			UndoSystem::Begin();
			object->AddComponentInstance((*firstMatch)(object));
			ImGui::CloseCurrentPopup();
		}

		ImGui::Separator();
		// 見つからなければ、入力した名前でスクリプトを作れるようにする
		std::string createLabel = (search[0] != '\0' && matchCount == 0)
			? std::string("「") + search + "」という名前でスクリプトを作成..."
			: std::string("新しいスクリプトを作成...");
		if (ImGui::Selectable(createLabel.c_str()))
		{
			AssetBrowser::StartNewScript(search[0] != '\0' ? search : "NewBehaviour");
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::End();
}
