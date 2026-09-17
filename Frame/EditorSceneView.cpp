#include "main.h"
#include "EditorGUI.h"
#include "EditorGUIInternal.h"
#include "EditorUI.h"
#include "Manager.h"
#include "GameObject.h"
#include "Renderer.h"
#include "CameraComponent.h"
#include "Collider.h"
#include "MeshField.h"
#include "AssetBrowser.h"
#include "SceneGrid.h"
#include "Gizmo.h"
#include "ImGuizmo.h"
#include "imgui_internal.h"	// BeginDragDropTargetCustom
#include <cfloat>

//=============================================================
// シーンビュー
//=============================================================
//=============================================================
// Game ビュー（ゲームのカメラで映した画面。Play 中はここでゲームを操作する）
//=============================================================
void EditorGUI::DrawGameView()
{
	if (m_FocusWindow == 2) { ImGui::SetNextWindowFocus(); m_FocusWindow = 0; }

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	bool visible = ImGui::Begin("Game", &m_ShowGame, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleVar();
	m_GameVisible = visible;

	if (visible)
	{
		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 6.0f, ImGui::GetCursorPosY() + 3.0f));
		ImGui::TextDisabled("16:9  %d x %d", SCREEN_WIDTH, SCREEN_HEIGHT);
		if (CameraComponent* camera = CameraComponent::GetMain())
		{
			ImGui::SameLine();
			ImGui::TextDisabled("|  Camera: %s", camera->GetGameObject()->GetName().c_str());
		}

		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImVec2 size = EditorUI::FitAspect(avail, (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT);

		ImVec2 origin = ImGui::GetCursorScreenPos();
		ImVec2 min(origin.x + (avail.x - size.x) * 0.5f, origin.y + (avail.y - size.y) * 0.5f);
		ImVec2 max(min.x + size.x, min.y + size.y);

		// 余白は黒（レターボックス）
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), IM_COL32(20, 20, 22, 255));
		dl->AddImage((ImTextureID)(intptr_t)Renderer::GetViewTexture(Renderer::VIEW_GAME), min, max);

		// クリックでウィンドウが動かないよう、見えないボタンを置く
		ImGui::SetCursorScreenPos(min);
		ImGui::InvisibleButton("##GameImage", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		m_GameHovered = ImGui::IsItemHovered();

		if (m_PlayState != PlayState::Play)
		{
			const char* text = "▶ で再生すると、ここでゲームを操作できます";
			ImVec2 textSize = ImGui::CalcTextSize(text);
			dl->AddText(ImVec2(min.x + (size.x - textSize.x) * 0.5f, max.y - textSize.y - 8.0f), IM_COL32(255, 255, 255, 160), text);
		}
	}
	ImGui::End();
}

//=============================================================
// ドロップした場所（マウスから伸ばした光線が地面と交わる点。地面がなければカメラの前）
//=============================================================
Vector3 EditorGUI::GetDropPosition()
{
	XMMATRIX view, projection;
	if (!EditorInternal::GetActiveViewProjection(view, projection)) return Vector3(0.0f, 0.0f, 0.0f);

	Vector3 origin, direction;
	EditorInternal::ScreenRay(view * projection, origin, direction);

	MeshField* field = Manager::GetGameObject<MeshField>();

	// 光線を少しずつ進めて、地面の高さより下に入ったところを探す（起伏があっても置ける）
	Vector3 p = origin;
	const float step = 0.5f;
	for (int i = 0; i < 1000; i++)
	{
		float ground = field ? field->GetHeight(p) : 0.0f;
		if (p.y <= ground)
		{
			p.y = ground;
			return p;
		}
		p += direction * step;
	}

	// 地面に届かない（空を向いている）ときはカメラの 10m 前
	return origin + direction * 10.0f;
}

void EditorGUI::DrawSceneView()
{
	if (m_FocusWindow == 1) { ImGui::SetNextWindowFocus(); m_FocusWindow = 0; }

	// ギズモを掴んでいる間はウィンドウが動かないようにする（前のフレームの状態で判定）
	ImGuiWindowFlags flags = m_GizmoActive ? ImGuiWindowFlags_NoMove : 0;
	m_GizmoActive = false;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	bool visible = ImGui::Begin("Scene", &m_ShowScene, flags);
	m_SceneVisible = visible;
	ImGui::PopStyleVar();

	if (visible)
	{
		// ギズモの切り替え（W/E/R キーでも切り替えられる）
		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 6.0f, ImGui::GetCursorPosY() + 4.0f));
		DrawGizmoToolbar();

		// 画面の縦横比を保ったまま、ウィンドウに収まる大きさで表示する
		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImVec2 size = EditorUI::FitAspect(avail, (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT);

		// 中央寄せ
		ImVec2 cursor = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(cursor.x + (avail.x - size.x) * 0.5f, cursor.y + (avail.y - size.y) * 0.5f));

		ImVec2 min = ImGui::GetCursorScreenPos();
		ImVec2 max(min.x + size.x, min.y + size.y);
		m_SceneMin[0] = min.x; m_SceneMin[1] = min.y;
		m_SceneMax[0] = max.x; m_SceneMax[1] = max.y;

		m_SceneDrawList = ImGui::GetWindowDrawList();
		m_SceneDrawList->AddImage((ImTextureID)(intptr_t)Renderer::GetSceneTexture(), min, max);

		// ボタンなどの項目に頼らず、ウィンドウと四角の範囲でマウスが乗っているか判定する
		m_SceneHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
			&& ImGui::IsMouseHoveringRect(min, max);

		// キーでギズモ切り替え（右ドラッグ中は WASD でカメラ移動するので切り替えない）
		ImGuiIO& io = ImGui::GetIO();
		if (m_SceneHovered && !ImGui::IsMouseDown(ImGuiMouseButton_Right) && !io.WantTextInput)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_W, false)) m_GizmoOperation = 0;
			if (ImGui::IsKeyPressed(ImGuiKey_E, false)) m_GizmoOperation = 1;
			if (ImGui::IsKeyPressed(ImGuiKey_R, false)) m_GizmoOperation = 2;
			if (ImGui::IsKeyPressed(ImGuiKey_F, false)) FocusObject(m_SelectedID);	// F: 選択中のものに寄る
		}

		// 止めている間は、クリックでオブジェクトを選ぶ（ギズモを掴んだときは選び直さない）
		if (UseEditorCamera() && m_SceneHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
			&& !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
		{
			PickObject();
		}

		// ギズモはボタンより先に処理する
		// （ImGuizmo は「他の ImGui の項目にマウスが乗っていない」ときしか掴めないため）
		DrawTransformGizmo();

		// 画像の上をドラッグしてもウィンドウが動かないよう、見えないボタンを置く
		// ギズモにマウスが乗っている間は置かない（置くとギズモが掴めなくなる）
		ImGui::SetCursorScreenPos(min);
		if (m_GizmoActive)
			ImGui::Dummy(size);
		else
			ImGui::InvisibleButton("##SceneImage", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

		// Project ウィンドウからのドロップ（ボタンの有無に関係なく、画像の範囲で受け取る）
		if (ImGui::BeginDragDropTargetCustom(ImRect(min, max), ImGui::GetID("##SceneDrop")))
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
			{
				std::string path = (const char*)payload->Data;

				if (AssetBrowser::GetType(path) == AssetBrowser::AssetType::Scene)
				{
					OpenScene(path);
				}
				else if (GameObject* object = AssetBrowser::CreateObject(path, GetDropPosition()))
				{
					m_SelectedID = object->GetID();
				}
			}
			ImGui::EndDragDropTarget();
		}

		// 左上に操作ヒント
		if (UseEditorCamera())
		{
			m_SceneDrawList->AddText(ImVec2(min.x + 8.0f, min.y + 6.0f), IM_COL32(255, 255, 255, 200),
				"クリック: 選択  右ドラッグ: 視点  右ドラッグ+WASD/QE: 移動  ホイール: 前後  F: フォーカス  Ctrl: スナップ");
		}
	}
	ImGui::End();
}

//=============================================================
// ギズモ切り替えのアイコンボタン（移動・回転・拡縮・ローカル/ワールド）
//=============================================================
void EditorGUI::DrawGizmoToolbar()
{
	if (EditorUI::IconButton("##Move", EditorUI::Icon::Move, m_GizmoOperation == 0, "移動 (W)"))   m_GizmoOperation = 0;
	ImGui::SameLine();
	if (EditorUI::IconButton("##Rotate", EditorUI::Icon::Rotate, m_GizmoOperation == 1, "回転 (E)")) m_GizmoOperation = 1;
	ImGui::SameLine();
	if (EditorUI::IconButton("##Scale", EditorUI::Icon::Scale, m_GizmoOperation == 2, "拡縮 (R)"))  m_GizmoOperation = 2;
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	if (EditorUI::IconButton("##Space", m_GizmoLocal ? EditorUI::Icon::Local : EditorUI::Icon::World, false,
		m_GizmoLocal ? "ローカル座標（押すとワールド座標）" : "ワールド座標（押すとローカル座標）"))
	{
		m_GizmoLocal = !m_GizmoLocal;
	}

	// グリッドの表示切り替え（井桁のアイコン）
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	if (EditorUI::IconButton("##Grid", EditorUI::Icon::Grid, SceneGrid::IsEnable(), "グリッド")) SceneGrid::SetEnable(!SceneGrid::IsEnable());

	// ギズモ（コライダーの線など）の表示切り替え
	ImGui::SameLine();
	{
		bool on = Gizmo::IsEnable();
		if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		if (ImGui::Button("Gizmos")) Gizmo::SetEnable(!on);
		if (on) ImGui::PopStyleColor();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("コライダーやカメラの線 (F1)");
	}
}

//=============================================================
// 選択中のオブジェクトのギズモ（ImGuizmo）
//=============================================================
void EditorGUI::DrawTransformGizmo()
{
	GameObject* object = Manager::FindGameObjectByID(m_SelectedID);
	if (object == nullptr) return;
	if (object->GetLayer() == 3) return;	// 2D のオブジェクトは対象外

	XMMATRIX view, projection;
	if (!EditorInternal::GetActiveViewProjection(view, projection)) return;

	XMFLOAT4X4 viewF, projectionF, worldF;
	XMStoreFloat4x4(&viewF, view);
	XMStoreFloat4x4(&projectionF, projection);
	XMStoreFloat4x4(&worldF, object->GetWorldMatrix());

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist(m_SceneDrawList);
	ImGuizmo::SetRect(m_SceneMin[0], m_SceneMin[1], m_SceneMax[0] - m_SceneMin[0], m_SceneMax[1] - m_SceneMin[1]);

	ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
	if (m_GizmoOperation == 1) operation = ImGuizmo::ROTATE;
	if (m_GizmoOperation == 2) operation = ImGuizmo::SCALE;

	// Ctrl を押している間はスナップ（移動 1m / 回転 15度 / 拡縮 0.1）
	float snap[3] = { 1.0f, 1.0f, 1.0f };
	if (m_GizmoOperation == 1) snap[0] = 15.0f;
	if (m_GizmoOperation == 2) snap[0] = snap[1] = snap[2] = 0.1f;
	bool useSnap = ImGui::GetIO().KeyCtrl;

	bool changed = ImGuizmo::Manipulate(&viewF._11, &projectionF._11, operation,
		m_GizmoLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD, &worldF._11, nullptr, useSnap ? snap : nullptr);

	m_GizmoActive = ImGuizmo::IsOver() || ImGuizmo::IsUsing();

	if (!changed) return;

	// ギズモが返すのはワールド行列なので、親がいれば親から見た行列に直す
	XMMATRIX local = XMLoadFloat4x4(&worldF);
	if (GameObject* parent = object->GetParent())
	{
		local = local * XMMatrixInverse(nullptr, parent->GetWorldMatrix());
	}

	XMVECTOR scale, rotation, translation;
	if (!XMMatrixDecompose(&scale, &rotation, &translation, local)) return;

	Vector3 position, scaleValue;
	XMStoreFloat3((XMFLOAT3*)&position, translation);
	XMStoreFloat3((XMFLOAT3*)&scaleValue, scale);

	object->SetPosition(position);
	if (m_GizmoOperation == 1) object->SetRotation(EditorInternal::RotationFromQuaternion(rotation));	// 回転以外では回転を書き換えない（誤差で角度の表記が変わるのを防ぐ）
	if (m_GizmoOperation == 2) object->SetScale(scaleValue);
}

//=============================================================
// シーンビューのクリックでオブジェクトを選ぶ
// 1. マウス位置から伸ばした光線とコライダーの箱が当たったもののうち、一番手前
// 2. どれにも当たらなければ、画面上でオブジェクトの位置に一番近いもの（25px 以内）
//=============================================================
void EditorGUI::PickObject()
{
	XMMATRIX view, projection;
	if (!EditorInternal::GetActiveViewProjection(view, projection)) return;

	ImVec2 mouse = ImGui::GetIO().MousePos;
	float width = m_SceneMax[0] - m_SceneMin[0];
	float height = m_SceneMax[1] - m_SceneMin[1];

	XMMATRIX viewProjection = view * projection;
	Vector3 origin, direction;
	EditorInternal::ScreenRay(viewProjection, origin, direction);

	unsigned int bestID = 0;
	float bestDistance = FLT_MAX;

	// 1. コライダーとの当たり（形はすべて外側の箱で近似する）
	for (GameObject* object : Manager::GetAllGameObjects())
	{
		if (object->IsDestroyed() || !object->IsActiveInHierarchy()) continue;

		for (Component* component : object->GetComponents())
		{
			Collider* collider = dynamic_cast<Collider*>(component);
			if (collider == nullptr || !collider->IsEnabled()) continue;

			ColliderShape shape = collider->GetShape();
			Vector3 halfExtents = shape.GetHalfExtents();
			float half[3] = { halfExtents.x, halfExtents.y, halfExtents.z };
			float center[3] = { shape.Center.x, shape.Center.y, shape.Center.z };
			float o[3] = { origin.x, origin.y, origin.z };
			float d[3] = { direction.x, direction.y, direction.z };

			// スラブ法で光線と箱の交差を調べる
			float tMin = 0.0f, tMax = FLT_MAX;
			bool hit = true;
			for (int axis = 0; axis < 3; axis++)
			{
				float lo = center[axis] - half[axis];
				float hi = center[axis] + half[axis];
				if (fabsf(d[axis]) < 1e-6f)
				{
					if (o[axis] < lo || o[axis] > hi) { hit = false; break; }
				}
				else
				{
					float t1 = (lo - o[axis]) / d[axis];
					float t2 = (hi - o[axis]) / d[axis];
					if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
					if (t1 > tMin) tMin = t1;
					if (t2 < tMax) tMax = t2;
					if (tMin > tMax) { hit = false; break; }
				}
			}

			if (hit && tMin < bestDistance)
			{
				bestDistance = tMin;
				bestID = object->GetID();
			}
		}
	}

	// 2. 画面上の距離
	if (bestID == 0)
	{
		float bestPixels = 25.0f;
		for (GameObject* object : Manager::GetAllGameObjects())
		{
			if (object->IsDestroyed() || !object->IsActiveInHierarchy() || object->GetLayer() == 3) continue;

			Vector3 p = object->GetWorldPosition();
			XMVECTOR clip = XMVector4Transform(XMVectorSet(p.x, p.y, p.z, 1.0f), viewProjection);
			float w = XMVectorGetW(clip);
			if (w <= 0.1f) continue;	// カメラの後ろ

			float sx = m_SceneMin[0] + (XMVectorGetX(clip) / w * 0.5f + 0.5f) * width;
			float sy = m_SceneMin[1] + (-XMVectorGetY(clip) / w * 0.5f + 0.5f) * height;
			float pixels = sqrtf((sx - mouse.x) * (sx - mouse.x) + (sy - mouse.y) * (sy - mouse.y));
			if (pixels < bestPixels)
			{
				bestPixels = pixels;
				bestID = object->GetID();
			}
		}
	}

	m_SelectedID = bestID;	// 何もないところをクリックしたら選択を外す
}
