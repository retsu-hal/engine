#include "main.h"
#include "EditorGUI.h"
#include "Manager.h"
#include "GameObject.h"
#include "Profiler.h"	// TypeName
#include "Renderer.h"
#include "Camera.h"
#include "EditorCamera.h"
#include "Collider.h"
#include "ImGuizmo.h"
#include "SceneSerializer.h"
#include "Registry.h"
#include "AssetBrowser.h"
#include "Prefabs.h"
#include "MeshField.h"
#include "imgui_internal.h"	// BeginDragDropTargetCustom
#include <typeinfo>
#include <cstring>
#include <cfloat>
#include <algorithm>

unsigned int EditorGUI::m_SelectedID = 0;
GameObject* EditorGUI::m_DragChild = nullptr;
GameObject* EditorGUI::m_DropParent = nullptr;
bool         EditorGUI::m_HasDrop = false;

PlayState    EditorGUI::m_PlayState = PlayState::Play;	// 起動直後はこれまでどおりゲームを動かす
int          EditorGUI::m_StepFrames = 0;
bool         EditorGUI::m_SceneHovered = false;
ImDrawList*  EditorGUI::m_SceneDrawList = nullptr;
float        EditorGUI::m_SceneMin[2] = { 0.0f, 0.0f };
float        EditorGUI::m_SceneMax[2] = { 0.0f, 0.0f };
int          EditorGUI::m_GizmoOperation = 0;
bool         EditorGUI::m_GizmoLocal = false;
bool         EditorGUI::m_GizmoActive = false;
std::string  EditorGUI::m_ScenePath = "asset\\scene\\GameScene.json";
std::string  EditorGUI::m_PlaySnapshot;
bool         EditorGUI::m_HasSnapshot = false;
char         EditorGUI::m_SaveAsBuffer[260] = "";
bool         EditorGUI::m_OpenSaveAsPopup = false;

//=============================================================
// 今シーンビューに映しているカメラの行列（止めている間はエディタカメラ、Play 中はゲームカメラ）
//=============================================================
static bool GetActiveViewProjection(XMMATRIX& view, XMMATRIX& projection)
{
	if (EditorGUI::UseEditorCamera() && EditorCamera::IsInitialized())
	{
		view = EditorCamera::GetViewMatrix();
		projection = EditorCamera::GetProjectionMatrix();
		return true;
	}

	if (CAMERA* camera = Manager::GetGameObject<CAMERA>())
	{
		view = camera->GetViewMatrix();
		projection = camera->GetProjectionMatrix();
		return true;
	}
	return false;
}

//=============================================================
// 回転行列 → GameObject の回転（XMMatrixRotationRollPitchYaw と同じ並び。x=pitch y=yaw z=roll）
//=============================================================
static Vector3 RotationFromQuaternion(FXMVECTOR quaternion)
{
	XMFLOAT4X4 m;
	XMStoreFloat4x4(&m, XMMatrixRotationQuaternion(quaternion));

	float sinPitch = -m._32;
	if (sinPitch > 1.0f)  sinPitch = 1.0f;
	if (sinPitch < -1.0f) sinPitch = -1.0f;
	float pitch = asinf(sinPitch);

	float yaw, roll;
	if (fabsf(sinPitch) < 0.9999f)
	{
		yaw = atan2f(m._31, m._33);
		roll = atan2f(m._12, m._22);
	}
	else
	{
		// 真上・真下を向いているときは yaw と roll が区別できないので roll を 0 にする
		yaw = atan2f(-m._13, m._11);
		roll = 0.0f;
	}
	return Vector3(pitch, yaw, roll);
}

void EditorGUI::Draw()
{
	DrawToolbar();
	DrawSceneView();
	DrawHierarchy();
	DrawInspector();
	AssetBrowser::Draw();
}

void EditorGUI::OpenScene(const std::string& path)
{
	m_PlayState = PlayState::Edit;
	m_HasSnapshot = false;
	m_SelectedID = 0;
	m_ScenePath = path;
	Manager::LoadSceneFile(path);
}

bool EditorGUI::ConsumeGameUpdate()
{
	if (m_PlayState == PlayState::Play) return true;

	if (m_StepFrames > 0)
	{
		m_StepFrames--;
		return true;
	}
	return false;
}

//=============================================================
// ファイルメニュー（シーンの保存・読み込み）
//=============================================================
void EditorGUI::SaveScene(const std::string& path)
{
	CreateDirectoryA("asset\\scene", nullptr);	// なければ作る（あれば何もしない）

	int skipped = SceneSerializer::SaveToFile(path);
	if (skipped >= 0) m_ScenePath = path;
	if (skipped > 0)
	{
		OutputDebugStringA("[Scene] 登録されていないオブジェクトは保存されていません（出力ウィンドウを確認）\n");
	}
}

void EditorGUI::DrawFileMenu()
{
	ImGuiIO& io = ImGui::GetIO();

	// Ctrl+S で上書き保存（Play 中は保存しない。動いている途中の状態が残ってしまうため）
	bool canSave = (m_PlayState == PlayState::Edit);
	if (canSave && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) && !io.WantTextInput)
	{
		SaveScene(m_ScenePath);
	}

	if (ImGui::BeginMenu("ファイル"))
	{
		ImGui::TextDisabled("%s", m_ScenePath.c_str());
		ImGui::Separator();

		if (ImGui::MenuItem("シーンを保存", "Ctrl+S", false, canSave))
		{
			SaveScene(m_ScenePath);
		}

		if (ImGui::MenuItem("名前を付けて保存...", nullptr, false, canSave))
		{
			strncpy_s(m_SaveAsBuffer, m_ScenePath.c_str(), _TRUNCATE);
			m_OpenSaveAsPopup = true;
		}

		// asset\scene にある .json を一覧にする
		if (ImGui::BeginMenu("シーンを開く"))
		{
			WIN32_FIND_DATAA find;
			HANDLE handle = FindFirstFileA("asset\\scene\\*.json", &find);
			bool any = false;
			if (handle != INVALID_HANDLE_VALUE)
			{
				do
				{
					any = true;
					std::string path = std::string("asset\\scene\\") + find.cFileName;
					if (ImGui::MenuItem(find.cFileName, nullptr, path == m_ScenePath))
					{
						OpenScene(path);
					}
				} while (FindNextFileA(handle, &find));
				FindClose(handle);
			}
			if (!any) ImGui::TextDisabled("asset\\scene に .json がありません");
			ImGui::EndMenu();
		}

		ImGui::EndMenu();
	}

	// 名前を付けて保存のダイアログ（メニューの中では開けないので外で開く）
	if (m_OpenSaveAsPopup)
	{
		ImGui::OpenPopup("SaveSceneAs");
		m_OpenSaveAsPopup = false;
	}
	if (ImGui::BeginPopupModal("SaveSceneAs", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("保存先（プロジェクトのフォルダからの相対パス）");
		ImGui::SetNextItemWidth(400.0f);
		ImGui::InputText("##path", m_SaveAsBuffer, sizeof(m_SaveAsBuffer));

		if (ImGui::Button("保存", ImVec2(120.0f, 0.0f)))
		{
			SaveScene(m_SaveAsBuffer);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル", ImVec2(120.0f, 0.0f))) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

//=============================================================
// ツールバー（再生 / 一時停止 / 停止）
//=============================================================
void EditorGUI::DrawToolbar()
{
	if (!ImGui::BeginMainMenuBar()) return;

	DrawFileMenu();

	enum class Icon { Play, Pause, Stop };

	// 記号のボタン（フォントに記号がなくても表示できるよう、図形で描く）
	auto iconButton = [](const char* id, Icon icon, bool active, const char* tooltip) -> bool
	{
		float h = ImGui::GetFrameHeight();
		ImVec2 size(h * 1.4f, h);

		if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		bool pressed = ImGui::Button(id, size);
		if (active) ImGui::PopStyleColor();

		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);

		ImVec2 min = ImGui::GetItemRectMin();
		ImVec2 max = ImGui::GetItemRectMax();
		ImVec2 c((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
		float r = h * 0.28f;	// 記号の大きさ
		ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
		ImDrawList* dl = ImGui::GetWindowDrawList();

		switch (icon)
		{
		case Icon::Play:	// ▶
			dl->AddTriangleFilled(ImVec2(c.x - r * 0.8f, c.y - r), ImVec2(c.x - r * 0.8f, c.y + r), ImVec2(c.x + r, c.y), color);
			break;
		case Icon::Pause:	// ❚❚
			dl->AddRectFilled(ImVec2(c.x - r * 0.8f, c.y - r), ImVec2(c.x - r * 0.25f, c.y + r), color);
			dl->AddRectFilled(ImVec2(c.x + r * 0.25f, c.y - r), ImVec2(c.x + r * 0.8f, c.y + r), color);
			break;
		case Icon::Stop:	// ■
			dl->AddRectFilled(ImVec2(c.x - r * 0.85f, c.y - r * 0.85f), ImVec2(c.x + r * 0.85f, c.y + r * 0.85f), color);
			break;
		}
		return pressed;
	};

	// Unity のように中央に並べる
	float h = ImGui::GetFrameHeight();
	float groupWidth = h * 1.4f * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
	ImGui::SetCursorPosX((ImGui::GetWindowWidth() - groupWidth) * 0.5f);

	// ▶：止まっていれば再生。Play 中は押しても何もしない
	if (iconButton("##Play", Icon::Play, m_PlayState == PlayState::Play, "再生"))
	{
		// 止まっている状態から再生するときは、今のシーンを覚えておく（Stop でこの状態に戻す）
		if (m_PlayState == PlayState::Edit)
		{
			m_HasSnapshot = (SceneSerializer::SaveToText(m_PlaySnapshot) == 0);	// 保存できない物があれば使わない
		}
		m_PlayState = PlayState::Play;
	}

	// ❚❚：Play 中なら一時停止、一時停止中なら再開
	if (iconButton("##Pause", Icon::Pause, m_PlayState == PlayState::Pause, "一時停止"))
	{
		if (m_PlayState == PlayState::Play)       m_PlayState = PlayState::Pause;
		else if (m_PlayState == PlayState::Pause) m_PlayState = PlayState::Play;
	}

	// ■：シーンを読み込み直して最初の状態に戻す（読み込み後の1フレーム更新は Manager 側で行う）
	if (iconButton("##Stop", Icon::Stop, false, "停止（最初の状態に戻す）"))
	{
		m_PlayState = PlayState::Edit;
		m_StepFrames = 0;

		// Play 前に覚えた状態があればそこへ戻す。なければシーンを最初から読み直す
		if (m_HasSnapshot) Manager::LoadSceneText(m_PlaySnapshot);
		else               Manager::ReloadScene();
		m_HasSnapshot = false;
		m_SelectedID = 0;
	}

	// 右端に状態と FPS
	const char* stateText = "Edit";
	if (m_PlayState == PlayState::Play)  stateText = "Playing";
	if (m_PlayState == PlayState::Pause) stateText = "Paused";
	char info[64];
	snprintf(info, sizeof(info), "%s  |  %.1f FPS", stateText, ImGui::GetIO().Framerate);
	ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(info).x - ImGui::GetStyle().WindowPadding.x * 2.0f);
	ImGui::TextDisabled("%s", info);

	ImGui::EndMainMenuBar();
}

//=============================================================
// シーンビュー
//=============================================================
//=============================================================
// ドロップした場所（マウスから伸ばした光線が地面と交わる点。地面がなければカメラの前）
//=============================================================
Vector3 EditorGUI::GetDropPosition()
{
	XMMATRIX view, projection;
	if (!GetActiveViewProjection(view, projection)) return Vector3(0.0f, 0.0f, 0.0f);

	ImVec2 mouse = ImGui::GetIO().MousePos;
	float ndcX = (mouse.x - m_SceneMin[0]) / (m_SceneMax[0] - m_SceneMin[0]) * 2.0f - 1.0f;
	float ndcY = 1.0f - (mouse.y - m_SceneMin[1]) / (m_SceneMax[1] - m_SceneMin[1]) * 2.0f;

	XMMATRIX inverse = XMMatrixInverse(nullptr, view * projection);
	XMVECTOR nearPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), inverse);
	XMVECTOR farPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), inverse);

	Vector3 origin, direction;
	XMStoreFloat3((XMFLOAT3*)&origin, nearPoint);
	XMStoreFloat3((XMFLOAT3*)&direction, XMVector3Normalize(farPoint - nearPoint));

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
	m_SceneHovered = false;
	m_SceneDrawList = nullptr;

	// ギズモを掴んでいる間はウィンドウが動かないようにする（前のフレームの状態で判定）
	ImGuiWindowFlags flags = m_GizmoActive ? ImGuiWindowFlags_NoMove : 0;
	m_GizmoActive = false;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	bool visible = ImGui::Begin("Scene", nullptr, flags);
	ImGui::PopStyleVar();

	if (visible)
	{
		// ギズモの切り替え（W/E/R キーでも切り替えられる）
		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 6.0f, ImGui::GetCursorPosY() + 4.0f));
		ImGui::RadioButton("移動 (W)", &m_GizmoOperation, 0);	ImGui::SameLine();
		ImGui::RadioButton("回転 (E)", &m_GizmoOperation, 1);	ImGui::SameLine();
		ImGui::RadioButton("拡縮 (R)", &m_GizmoOperation, 2);	ImGui::SameLine();
		ImGui::TextDisabled("|");	ImGui::SameLine();
		ImGui::Checkbox("Local", &m_GizmoLocal);

		// 画面の縦横比を保ったまま、ウィンドウに収まる大きさで表示する
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float aspect = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;
		ImVec2 size(avail.x, avail.x / aspect);
		if (size.y > avail.y) size = ImVec2(avail.y * aspect, avail.y);
		if (size.x < 1.0f || size.y < 1.0f) size = ImVec2(1.0f, 1.0f);

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
// 選択中のオブジェクトのギズモ（ImGuizmo）
//=============================================================
void EditorGUI::DrawTransformGizmo()
{
	GameObject* object = Manager::FindGameObjectByID(m_SelectedID);
	if (object == nullptr) return;
	if (object->GetLayer() == 3) return;	// 2D のオブジェクトは対象外

	XMMATRIX view, projection;
	if (!GetActiveViewProjection(view, projection)) return;

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
	if (m_GizmoOperation == 1) object->SetRotation(RotationFromQuaternion(rotation));	// 回転以外では回転を書き換えない（誤差で角度の表記が変わるのを防ぐ）
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
	if (!GetActiveViewProjection(view, projection)) return;

	ImVec2 mouse = ImGui::GetIO().MousePos;
	float width = m_SceneMax[0] - m_SceneMin[0];
	float height = m_SceneMax[1] - m_SceneMin[1];
	float ndcX = (mouse.x - m_SceneMin[0]) / width * 2.0f - 1.0f;
	float ndcY = 1.0f - (mouse.y - m_SceneMin[1]) / height * 2.0f;

	XMMATRIX viewProjection = view * projection;
	XMMATRIX inverse = XMMatrixInverse(nullptr, viewProjection);
	XMVECTOR nearPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), inverse);
	XMVECTOR farPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), inverse);

	XMFLOAT3 origin, direction;
	XMStoreFloat3(&origin, nearPoint);
	XMStoreFloat3(&direction, XMVector3Normalize(farPoint - nearPoint));

	unsigned int bestID = 0;
	float bestDistance = FLT_MAX;

	// 1. コライダーとの当たり（形はすべて外側の箱で近似する）
	for (GameObject* object : Manager::GetAllGameObjects())
	{
		if (object->IsDestroyed()) continue;

		for (Component* component : object->GetComponents())
		{
			Collider* collider = dynamic_cast<Collider*>(component);
			if (collider == nullptr || !collider->IsEnabled()) continue;

			ColliderShape shape = collider->GetShape();
			float half[3] = {
				shape.HalfX + shape.RadiusXZ + shape.Radius,
				shape.HalfY + shape.Radius,
				shape.HalfZ + shape.RadiusXZ + shape.Radius };
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
			if (object->IsDestroyed() || object->GetLayer() == 3) continue;

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

	// 空いている場所を右クリック：オブジェクトを作る
	int createType = -1;	// 0:空 1:四角 2:球 3:カプセル
	if (ImGui::BeginPopupContextItem("##HierarchyContext"))
	{
		if (ImGui::MenuItem("空のオブジェクト")) createType = 0;
		ImGui::Separator();
		if (ImGui::MenuItem("四角"))       createType = 1;
		if (ImGui::MenuItem("球"))         createType = 2;
		if (ImGui::MenuItem("カプセル"))   createType = 3;
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

	if (createType >= 0)
	{
		// エディタカメラの 10m 前に置く
		Vector3 position(0.0f, 0.0f, 0.0f);
		if (EditorCamera::IsInitialized()) position = EditorCamera::GetPosition() + EditorCamera::GetForward() * 10.0f;

		GameObject* object = nullptr;
		switch (createType)
		{
		case 0: object = Prefabs::CreateEmpty(position);   break;
		case 1: object = Prefabs::CreateCube(position);    break;
		case 2: object = Prefabs::CreateSphere(position);  break;
		case 3: object = Prefabs::CreateCapsule(position); break;
		}
		if (object) m_SelectedID = object->GetID();
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
		FocusObject(m_SelectedID);	// Scene のカメラを寄せる
	}

	// 右クリック：削除（子も一緒に消える）
	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem("削除")) object->SetDestroy();
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
		if (ImGui::CollapsingHeader("Editor Camera"))
		{
			EditorCamera::OnInspectorGUI();
		}
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

	// コンポーネントを追加（登録されているものを一覧から選ぶ）
	ImGui::Separator();
	if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f))) ImGui::OpenPopup("AddComponentPopup");
	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		for (auto& pair : ComponentRegistry::GetAll())
		{
			if (ImGui::MenuItem(pair.first.c_str()))
			{
				object->AddComponentInstance(pair.second(object));
			}
		}
		ImGui::EndPopup();
	}

	ImGui::End();
}

//=============================================================
// Scene のカメラをオブジェクトに寄せる
// コライダーがあればその大きさ、なければ 1m 程度の物として距離を決める
//=============================================================
void EditorGUI::FocusObject(unsigned int id)
{
	if (!UseEditorCamera() || !EditorCamera::IsInitialized()) return;	// Play 中はゲームカメラなので何もしない

	GameObject* object = Manager::FindGameObjectByID(id);
	if (object == nullptr) return;

	Vector3 center = object->GetWorldPosition();
	float radius = 1.0f;

	// コライダーをすべて囲む箱を求める
	bool found = false;
	Vector3 boundsMin, boundsMax;
	for (Component* component : object->GetComponents())
	{
		Collider* collider = dynamic_cast<Collider*>(component);
		if (collider == nullptr) continue;

		ColliderShape shape = collider->GetShape();
		Vector3 half(shape.HalfX + shape.RadiusXZ + shape.Radius,
			shape.HalfY + shape.Radius,
			shape.HalfZ + shape.RadiusXZ + shape.Radius);
		Vector3 mn = shape.Center - half;
		Vector3 mx = shape.Center + half;

		if (!found)
		{
			boundsMin = mn;
			boundsMax = mx;
			found = true;
		}
		else
		{
			boundsMin = Vector3((std::min)(boundsMin.x, mn.x), (std::min)(boundsMin.y, mn.y), (std::min)(boundsMin.z, mn.z));
			boundsMax = Vector3((std::max)(boundsMax.x, mx.x), (std::max)(boundsMax.y, mx.y), (std::max)(boundsMax.z, mx.z));
		}
	}

	if (found)
	{
		center = (boundsMin + boundsMax) * 0.5f;
		radius = (boundsMax - boundsMin).length() * 0.5f;
	}

	// 大きい物ほど離れる（視野角 60 度に収まる距離の目安）。近すぎ・遠すぎは制限
	float distance = radius * 2.5f;
	if (distance < 3.0f)   distance = 3.0f;
	if (distance > 200.0f) distance = 200.0f;

	EditorCamera::Focus(center, distance);
}
