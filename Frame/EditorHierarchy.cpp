#include "main.h"
#include "EditorGUI.h"
#include "Manager.h"
#include "GameObject.h"

//=============================================================
// Hierarchy
//=============================================================
void EditorGUI::DrawHierarchy()
{
	ImGui::Begin("Hierarchy", &m_ShowHierarchy);

	m_HasDrop = false;

	// 親がいないオブジェクトから木構造をたどる
	for (GameObject* object : Manager::GetAllGameObjects())
	{
		if (object->GetParent() == nullptr && !object->IsDestroyed())
		{
			DrawNode(object);
		}
	}

	// 木の下の空いている場所にドロップしたら親子解除
	ImVec2 rest = ImGui::GetContentRegionAvail();
	if (rest.y < 20.0f) rest.y = 20.0f;
	ImGui::InvisibleButton("##HierarchyEmpty", rest);

	// 空いている場所を右クリック：オブジェクトを作る
	ObjectType createType = ObjectType::None;
	if (ImGui::BeginPopupContextItem("##HierarchyContext"))
	{
		createType = DrawCreateObjectMenuItems(false);
		ImGui::EndPopup();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT_ID"))
		{
			unsigned int id = *(const unsigned int*)payload->Data;
			m_DragChild = Manager::FindGameObjectByID(id);
			m_DropParent = nullptr;
			m_HasDrop = (m_DragChild != nullptr);
		}
		ImGui::EndDragDropTarget();
	}

	// 木をたどっている最中に m_Children を書き換えると壊れるので、描き終わってから反映する
	if (m_HasDrop)
	{
		m_DragChild->SetParent(m_DropParent);
		m_HasDrop = false;
	}

	CreateAndSelect(createType);

	ImGui::End();
}

void EditorGUI::DrawNode(GameObject* object)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (object->GetChildren().empty()) flags |= ImGuiTreeNodeFlags_Leaf;
	if (object->GetID() == m_SelectedID) flags |= ImGuiTreeNodeFlags_Selected;

	ImGui::PushID((int)object->GetID());

	bool renaming = (m_RenamingID == object->GetID());
	bool inactive = !object->IsActiveInHierarchy();	// 非アクティブは灰色で表示
	if (inactive) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	bool open = ImGui::TreeNodeEx("##node", flags, "%s", renaming ? "" : object->GetName().c_str());
	if (inactive) ImGui::PopStyleColor();

	// 名前の変更中は、名前の位置に入力欄を出す
	if (renaming)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		if (m_RenameFocus)
		{
			ImGui::SetKeyboardFocusHere();
			m_RenameFocus = false;
		}
		bool enter = ImGui::InputText("##rename", m_RenameBuffer, sizeof(m_RenameBuffer),
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			m_RenamingID = 0;	// やめる
		}
		else if (enter || ImGui::IsItemDeactivated())
		{
			if (m_RenameBuffer[0] != '\0') object->SetName(m_RenameBuffer);
			m_RenamingID = 0;
		}
	}

	// クリックで選択
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		m_SelectedID = object->GetID();
		FocusObject(m_SelectedID);	// Scene のカメラを寄せる
	}

	// 右クリック：削除（子も一緒に消える）
	if (ImGui::BeginPopupContextItem())
	{
		m_SelectedID = object->GetID();
		// 木をたどっている最中に親子関係を変えると壊れるので、複製は描き終わってから行う
		if (ImGui::MenuItem("名前の変更", "F2")) BeginRename(object->GetID());
		if (ImGui::MenuItem("複製", "Ctrl+D")) m_DuplicateRequested = true;
		if (ImGui::MenuItem("アクティブ切り替え", "Alt+Shift+A")) object->SetActive(!object->IsActiveSelf());
		if (ImGui::MenuItem("削除", "Delete"))
		{
			object->SetDestroy();
			m_SelectedID = 0;
		}
		ImGui::EndPopup();
	}

	// ドラッグ元
	if (ImGui::BeginDragDropSource())
	{
		unsigned int id = object->GetID();
		ImGui::SetDragDropPayload("GAMEOBJECT_ID", &id, sizeof(id));
		ImGui::Text("%s", object->GetName().c_str());
		ImGui::EndDragDropSource();
	}

	// ドロップ先（このオブジェクトの子にする）
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT_ID"))
		{
			unsigned int id = *(const unsigned int*)payload->Data;
			m_DragChild = Manager::FindGameObjectByID(id);
			m_DropParent = object;
			m_HasDrop = (m_DragChild != nullptr);
		}
		ImGui::EndDragDropTarget();
	}

	if (open)
	{
		for (GameObject* child : object->GetChildren())
		{
			if (!child->IsDestroyed()) DrawNode(child);
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}
