#include "main.h"
#include "EditorGUI.h"
#include "Manager.h"
#include "GameObject.h"
#include "Profiler.h"	// TypeName
#include <typeinfo>
#include <cstring>

unsigned int EditorGUI::m_SelectedID = 0;
GameObject* EditorGUI::m_DragChild = nullptr;
GameObject* EditorGUI::m_DropParent = nullptr;
bool         EditorGUI::m_HasDrop = false;

void EditorGUI::Draw()
{
	DrawHierarchy();
	DrawInspector();
}

//=============================================================
// Hierarchy
//=============================================================
void EditorGUI::DrawHierarchy()
{
	ImGui::Begin("Hierarchy");

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

	ImGui::End();
}

void EditorGUI::DrawNode(GameObject* object)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (object->GetChildren().empty()) flags |= ImGuiTreeNodeFlags_Leaf;
	if (object->GetID() == m_SelectedID) flags |= ImGuiTreeNodeFlags_Selected;

	ImGui::PushID((int)object->GetID());
	bool open = ImGui::TreeNodeEx("##node", flags, "%s", object->GetName().c_str());

	// クリックで選択
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		m_SelectedID = object->GetID();
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

//=============================================================
// Inspector
//=============================================================
void EditorGUI::DrawInspector()
{
	ImGui::Begin("Inspector");

	GameObject* object = Manager::FindGameObjectByID(m_SelectedID);
	if (object == nullptr)
	{
		ImGui::TextDisabled("Hierarchy でオブジェクトを選択してください");
		ImGui::End();
		return;
	}

	// 名前
	char name[128];
	strncpy_s(name, object->GetName().c_str(), _TRUNCATE);
	if (ImGui::InputText("Name", name, sizeof(name)))
	{
		object->SetName(name);
	}

	ImGui::TextDisabled("ID: %u   Class: %s", object->GetID(), TypeName(typeid(*object).name()).c_str());

	GameObject* parent = object->GetParent();
	ImGui::Text("Parent: %s", parent ? parent->GetName().c_str() : "(なし)");
	if (parent && ImGui::SmallButton("親子関係を解除"))
	{
		object->SetParent(nullptr);
	}

	// Transform
	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		Vector3 pos = object->GetPosition();
		if (ImGui::DragFloat3("Position", &pos.x, 0.05f)) object->SetPosition(pos);

		// 内部はラジアン、表示は度
		Vector3 rot = object->GetRotation();
		float deg[3] = { XMConvertToDegrees(rot.x), XMConvertToDegrees(rot.y), XMConvertToDegrees(rot.z) };
		if (ImGui::DragFloat3("Rotation", deg, 0.5f))
		{
			object->SetRotation({ XMConvertToRadians(deg[0]), XMConvertToRadians(deg[1]), XMConvertToRadians(deg[2]) });
		}

		Vector3 scale = object->GetScale();
		if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) object->SetScale(scale);
	}

	// コンポーネント
	int index = 0;
	for (Component* component : object->GetComponents())
	{
		ImGui::PushID(index++);
		std::string label = TypeName(typeid(*component).name());
		if (ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			bool enabled = component->IsEnabled();
			if (ImGui::Checkbox("Enabled", &enabled)) component->SetEnabled(enabled);

			component->OnInspectorGUI();
		}
		ImGui::PopID();
	}

	ImGui::End();
}
