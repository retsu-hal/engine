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
#include "UndoSystem.h"
#include "CameraComponent.h"
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
GizmoOperation EditorGUI::m_GizmoOperation = GizmoOperation::Translate;
bool         EditorGUI::m_GizmoLocal = false;
bool         EditorGUI::m_GizmoActive = false;
std::string  EditorGUI::m_ScenePath = "asset\\scene\\GameScene.json";
std::string  EditorGUI::m_PlaySnapshot;
bool         EditorGUI::m_HasSnapshot = false;
char         EditorGUI::m_SaveAsBuffer[260] = "";
bool         EditorGUI::m_OpenSaveAsPopup = false;
bool         EditorGUI::m_DuplicateRequested = false;
unsigned int EditorGUI::m_RenamingID = 0;
char         EditorGUI::m_RenameBuffer[128] = "";
bool         EditorGUI::m_RenameFocus = false;
std::string  EditorGUI::m_RestoreSelectName;

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

	if (CameraComponent* camera = CameraComponent::GetMain())
	{
		view = camera->GetViewMatrix();
		projection = camera->GetProjectionMatrix();
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

// 2D のオブジェクト（Layer 3）はシーンビューの選択・ギズモの対象にしない
static const int LAYER_2D = 3;

//=============================================================
// マウス位置から奥に伸ばした光線
// シーンビューの四角の中での位置を NDC に直し、ビュー射影の逆行列で世界に戻す
//=============================================================
static bool ScreenRayFromMouse(const float rectMin[2], const float rectMax[2],
	Vector3& outOrigin, Vector3& outDirection, XMMATRIX* outViewProjection = nullptr)
{
	XMMATRIX view, projection;
	if (!GetActiveViewProjection(view, projection)) return false;

	ImVec2 mouse = ImGui::GetIO().MousePos;
	float ndcX = (mouse.x - rectMin[0]) / (rectMax[0] - rectMin[0]) * 2.0f - 1.0f;
	float ndcY = 1.0f - (mouse.y - rectMin[1]) / (rectMax[1] - rectMin[1]) * 2.0f;

	XMMATRIX viewProjection = view * projection;
	XMMATRIX inverse = XMMatrixInverse(nullptr, viewProjection);
	XMVECTOR nearPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), inverse);
	XMVECTOR farPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), inverse);

	XMStoreFloat3((XMFLOAT3*)&outOrigin, nearPoint);
	XMStoreFloat3((XMFLOAT3*)&outDirection, XMVector3Normalize(farPoint - nearPoint));
	if (outViewProjection) *outViewProjection = viewProjection;
	return true;
}

//=============================================================
// コライダーの形を囲む箱（どの形も「四角＋円柱＋丸み」を足した大きさで近似する）
//=============================================================
static void ColliderBounds(const ColliderShape& shape, Vector3& outMin, Vector3& outMax)
{
	Vector3 half(shape.HalfX + shape.RadiusXZ + shape.Radius,
		shape.HalfY + shape.Radius,
		shape.HalfZ + shape.RadiusXZ + shape.Radius);
	outMin = shape.Center - half;
	outMax = shape.Center + half;
}

//=============================================================
// 記号のボタン（フォントに記号がなくても表示できるよう、図形で描く）
//=============================================================
enum class EditorIcon { Play, Pause, Stop, Move, Rotate, Scale, Local, World };

static void DrawIconSymbol(ImDrawList* dl, EditorIcon icon, ImVec2 c, float r, ImU32 color);

static bool IconButton(const char* id, EditorIcon icon, bool active, const char* tooltip,
	float widthScale, float radiusScale)
{
	float h = ImGui::GetFrameHeight();

	if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	bool pressed = ImGui::Button(id, ImVec2(h * widthScale, h));
	if (active) ImGui::PopStyleColor();

	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);

	ImVec2 min = ImGui::GetItemRectMin();
	ImVec2 max = ImGui::GetItemRectMax();
	ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
	DrawIconSymbol(ImGui::GetWindowDrawList(), icon, center, h * radiusScale, ImGui::GetColorU32(ImGuiCol_Text));
	return pressed;
}

// ツールバーとギズモ切り替えでボタンと記号の大きさが少し違う
static bool ToolbarIconButton(const char* id, EditorIcon icon, bool active, const char* tooltip)
{
	return IconButton(id, icon, active, tooltip, 1.4f, 0.28f);
}

static bool GizmoIconButton(const char* id, EditorIcon icon, bool active, const char* tooltip)
{
	return IconButton(id, icon, active, tooltip, 1.3f, 0.32f);
}

// c: 中心  r: 記号の大きさ
static void DrawIconSymbol(ImDrawList* dl, EditorIcon icon, ImVec2 c, float r, ImU32 color)
{
	const float t = 1.5f;	// 線の太さ

	switch (icon)
	{
	case EditorIcon::Play:		// ▶
		dl->AddTriangleFilled(ImVec2(c.x - r * 0.8f, c.y - r), ImVec2(c.x - r * 0.8f, c.y + r), ImVec2(c.x + r, c.y), color);
		break;
	case EditorIcon::Pause:		// ❚❚
		dl->AddRectFilled(ImVec2(c.x - r * 0.8f, c.y - r), ImVec2(c.x - r * 0.25f, c.y + r), color);
		dl->AddRectFilled(ImVec2(c.x + r * 0.25f, c.y - r), ImVec2(c.x + r * 0.8f, c.y + r), color);
		break;
	case EditorIcon::Stop:		// ■
		dl->AddRectFilled(ImVec2(c.x - r * 0.85f, c.y - r * 0.85f), ImVec2(c.x + r * 0.85f, c.y + r * 0.85f), color);
		break;
	case EditorIcon::Move:		// 十字の矢印
	{
		dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), color, t);
		dl->AddLine(ImVec2(c.x, c.y - r), ImVec2(c.x, c.y + r), color, t);
		float a = r * 0.35f;
		dl->AddTriangleFilled(ImVec2(c.x + r + 1, c.y), ImVec2(c.x + r - a, c.y - a), ImVec2(c.x + r - a, c.y + a), color);
		dl->AddTriangleFilled(ImVec2(c.x - r - 1, c.y), ImVec2(c.x - r + a, c.y + a), ImVec2(c.x - r + a, c.y - a), color);
		dl->AddTriangleFilled(ImVec2(c.x, c.y - r - 1), ImVec2(c.x + a, c.y - r + a), ImVec2(c.x - a, c.y - r + a), color);
		dl->AddTriangleFilled(ImVec2(c.x, c.y + r + 1), ImVec2(c.x - a, c.y + r - a), ImVec2(c.x + a, c.y + r - a), color);
		break;
	}
	case EditorIcon::Rotate:	// 回る矢印
	{
		dl->PathArcTo(c, r * 0.85f, XM_PI * 0.15f, XM_PI * 1.75f, 20);
		dl->PathStroke(color, 0, t);
		ImVec2 tip(c.x + cosf(XM_PI * 1.75f) * r * 0.85f, c.y + sinf(XM_PI * 1.75f) * r * 0.85f);
		float a = r * 0.4f;
		dl->AddTriangleFilled(ImVec2(tip.x + a, tip.y), ImVec2(tip.x - a * 0.3f, tip.y - a), ImVec2(tip.x - a * 0.3f, tip.y + a * 0.6f), color);
		break;
	}
	case EditorIcon::Scale:		// 小さい四角から大きい四角へ伸びる
	{
		dl->AddRect(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), color, 0.0f, 0, t);
		dl->AddRectFilled(ImVec2(c.x - r, c.y + r * 0.1f), ImVec2(c.x - r * 0.1f, c.y + r), color);
		dl->AddLine(ImVec2(c.x - r * 0.2f, c.y + r * 0.2f), ImVec2(c.x + r * 0.7f, c.y - r * 0.7f), color, t);
		float a = r * 0.35f;
		dl->AddTriangleFilled(ImVec2(c.x + r * 0.85f, c.y - r * 0.85f), ImVec2(c.x + r * 0.85f - a * 1.4f, c.y - r * 0.85f), ImVec2(c.x + r * 0.85f, c.y - r * 0.85f + a * 1.4f), color);
		break;
	}
	case EditorIcon::Local:		// 立方体（自分の向き）
	{
		float s = r * 0.7f, o = r * 0.4f;
		dl->AddRect(ImVec2(c.x - s - o * 0.5f, c.y - s + o * 0.5f), ImVec2(c.x + s - o * 0.5f, c.y + s + o * 0.5f), color, 0.0f, 0, t);
		dl->AddLine(ImVec2(c.x - s - o * 0.5f, c.y - s + o * 0.5f), ImVec2(c.x - s + o * 0.5f, c.y - s - o * 0.5f), color, t);
		dl->AddLine(ImVec2(c.x + s - o * 0.5f, c.y - s + o * 0.5f), ImVec2(c.x + s + o * 0.5f, c.y - s - o * 0.5f), color, t);
		dl->AddLine(ImVec2(c.x + s - o * 0.5f, c.y + s + o * 0.5f), ImVec2(c.x + s + o * 0.5f, c.y + s - o * 0.5f), color, t);
		dl->AddLine(ImVec2(c.x - s + o * 0.5f, c.y - s - o * 0.5f), ImVec2(c.x + s + o * 0.5f, c.y - s - o * 0.5f), color, t);
		dl->AddLine(ImVec2(c.x + s + o * 0.5f, c.y - s - o * 0.5f), ImVec2(c.x + s + o * 0.5f, c.y + s - o * 0.5f), color, t);
		break;
	}
	case EditorIcon::World:		// 地球（円と経線・緯線）
		dl->AddCircle(c, r, color, 20, t);
		dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), color, t);
		dl->AddEllipse(c, ImVec2(r * 0.45f, r), color, 0.0f, 20, t);
		break;
	}
}

void EditorGUI::Draw()
{
	TrackUndo(false);

	// 元に戻した直後：読み込みが終わったら、同じ名前のオブジェクトを選び直す
	if (!m_RestoreSelectName.empty() && !Manager::IsSceneChanging())
	{
		for (GameObject* object : Manager::GetAllGameObjects())
		{
			if (!object->IsDestroyed() && object->GetName() == m_RestoreSelectName)
			{
				m_SelectedID = object->GetID();
				break;
			}
		}
		m_RestoreSelectName.clear();
	}

	DrawToolbar();
	DrawSceneView();
	DrawHierarchy();
	DrawInspector();
	AssetBrowser::Draw();
	HandleShortcuts();

	TrackUndo(true);
}

//=============================================================
// 元に戻す用の記録
// マウスを押した瞬間の状態を覚え、離して操作中の項目がなくなったら比べる
// （ギズモ、Inspector の数値、Hierarchy のドラッグ、メニューからの作成などをまとめて扱える）
//=============================================================
void EditorGUI::TrackUndo(bool frameEnd)
{
	if (m_PlayState == PlayState::Play)
	{
		UndoSystem::Cancel();
		return;
	}

	if (!frameEnd)
	{
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			UndoSystem::Begin();
		return;
	}

	bool busy = ImGui::IsAnyMouseDown() || ImGui::IsAnyItemActive() || ImGuizmo::IsUsing() || m_RenamingID != 0;
	if (UndoSystem::IsPending() && !busy) UndoSystem::End();
}

//=============================================================
// 再生・一時停止・停止
//=============================================================
void EditorGUI::Play()
{
	// 止まっている状態から再生するときは、今のシーンを覚えておく（Stop でこの状態に戻す）
	if (m_PlayState == PlayState::Edit)
	{
		m_HasSnapshot = (SceneSerializer::SaveToText(m_PlaySnapshot) == 0);	// 保存できない物があれば使わない
	}
	UndoSystem::Cancel();
	m_PlayState = PlayState::Play;
}

void EditorGUI::TogglePause()
{
	if (m_PlayState == PlayState::Play)       m_PlayState = PlayState::Pause;
	else if (m_PlayState == PlayState::Pause) m_PlayState = PlayState::Play;
}

void EditorGUI::Stop()
{
	m_PlayState = PlayState::Edit;
	m_StepFrames = 0;

	// Play 前に覚えた状態があればそこへ戻す。なければシーンを最初から読み直す
	if (m_HasSnapshot) Manager::LoadSceneText(m_PlaySnapshot);
	else               Manager::ReloadScene();
	m_HasSnapshot = false;
	m_SelectedID = 0;
}

//=============================================================
// 元に戻す／やり直し
// 読み込み直しで ID が変わるので、選んでいたものは名前で選び直す（Draw の先頭で拾う）
//=============================================================
void EditorGUI::ApplyHistory(bool redo)
{
	if (GameObject* object = Manager::FindGameObjectByID(m_SelectedID)) m_RestoreSelectName = object->GetName();

	if (redo ? UndoSystem::Redo() : UndoSystem::Undo()) m_SelectedID = 0;
	else m_RestoreSelectName.clear();
}

void EditorGUI::BeginRename(unsigned int id)
{
	GameObject* object = Manager::FindGameObjectByID(id);
	if (object == nullptr) return;
	UndoSystem::Begin();	// 名前の変更も元に戻せるように
	m_RenamingID = id;
	strncpy_s(m_RenameBuffer, object->GetName().c_str(), _TRUNCATE);
	m_RenameFocus = true;
}

//=============================================================
// 編集メニュー
//=============================================================
void EditorGUI::DrawEditMenu()
{
	if (!ImGui::BeginMenu("編集")) return;

	bool editing = (m_PlayState != PlayState::Play);
	bool selected = (Manager::FindGameObjectByID(m_SelectedID) != nullptr);

	if (ImGui::MenuItem("元に戻す", "Ctrl+Z", false, editing && UndoSystem::CanUndo())) ApplyHistory(false);
	if (ImGui::MenuItem("やり直し", "Ctrl+Y", false, editing && UndoSystem::CanRedo())) ApplyHistory(true);
	ImGui::Separator();
	if (ImGui::MenuItem("複製", "Ctrl+D", false, selected)) m_DuplicateRequested = true;
	if (ImGui::MenuItem("名前の変更", "F2", false, selected)) BeginRename(m_SelectedID);
	if (ImGui::MenuItem("削除", "Delete", false, selected))
	{
		UndoSystem::Begin();
		Manager::FindGameObjectByID(m_SelectedID)->SetDestroy();
		m_SelectedID = 0;
	}
	ImGui::Separator();
	if (ImGui::MenuItem("フォーカス", "F", false, selected)) FocusObject(m_SelectedID);
	ImGui::Separator();
	if (ImGui::MenuItem(m_PlayState == PlayState::Play ? "停止" : "再生", "Ctrl+P"))
	{
		if (m_PlayState == PlayState::Edit) Play();
		else Stop();
	}
	if (ImGui::MenuItem("一時停止", "Ctrl+Shift+P", m_PlayState == PlayState::Pause, m_PlayState != PlayState::Edit)) TogglePause();

	ImGui::EndMenu();
}

//=============================================================
// ショートカット（Scene か Hierarchy にマウスがあるときだけ。文字入力中は無視）
//=============================================================
void EditorGUI::HandleShortcuts()
{
	ImGuiIO& io = ImGui::GetIO();
	bool typing = io.WantTextInput;

	//--------------------------------------------------------------
	// どこにマウスがあっても使えるもの
	//--------------------------------------------------------------
	if (!typing && io.KeyCtrl)
	{
		bool editing = (m_PlayState != PlayState::Play);

		// Ctrl+Z：元に戻す / Ctrl+Y・Ctrl+Shift+Z：やり直し
		bool undo = ImGui::IsKeyPressed(ImGuiKey_Z, false) && !io.KeyShift;
		bool redo = ImGui::IsKeyPressed(ImGuiKey_Y, false) || (ImGui::IsKeyPressed(ImGuiKey_Z, false) && io.KeyShift);
		if (editing && (undo || redo)) ApplyHistory(!undo);	// 同じフレームに両方来たら元に戻すを優先

		// Ctrl+P：再生／停止、Ctrl+Shift+P：一時停止
		if (ImGui::IsKeyPressed(ImGuiKey_P, false))
		{
			if (io.KeyShift) TogglePause();
			else if (m_PlayState == PlayState::Edit) Play();
			else Stop();
		}
	}

	//--------------------------------------------------------------
	// Scene か Hierarchy にマウスがあるときだけ使えるもの
	//--------------------------------------------------------------
	bool hierarchyHovered = false;
	if (ImGuiWindow* window = ImGui::FindWindowByName("Hierarchy"))
		hierarchyHovered = ImGui::GetCurrentContext()->HoveredWindow == window;

	bool allowKeys = (m_SceneHovered || hierarchyHovered) && !typing;
	bool duplicate = m_DuplicateRequested;
	m_DuplicateRequested = false;

	GameObject* selected = Manager::FindGameObjectByID(m_SelectedID);
	if (selected == nullptr) return;

	if (allowKeys && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) duplicate = true;

	// Ctrl+D（または右クリックの「複製」）：子ごと複製して、複製した方を選ぶ（名前は自動で (1) などが付く）
	if (duplicate)
	{
		UndoSystem::Begin();
		if (GameObject* copy = SceneSerializer::Duplicate(selected)) m_SelectedID = copy->GetID();
	}

	// Delete：削除（子も一緒に消える）
	if (allowKeys && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		UndoSystem::Begin();
		selected->SetDestroy();
		m_SelectedID = 0;
	}

	// F2：名前の変更
	if (allowKeys && ImGui::IsKeyPressed(ImGuiKey_F2, false)) BeginRename(m_SelectedID);
}

void EditorGUI::OpenScene(const std::string& path)
{
	m_PlayState = PlayState::Edit;
	m_HasSnapshot = false;
	m_SelectedID = 0;
	m_ScenePath = path;
	UndoSystem::Clear();
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
			// 今のファイル名（拡張子なし）を入れておく
			strncpy_s(m_SaveAsBuffer, AssetBrowser::GetStem(m_ScenePath).c_str(), _TRUNCATE);
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
		ImGui::Text("シーンの名前");
		ImGui::SetNextItemWidth(300.0f);
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		bool enter = ImGui::InputText("##name", m_SaveAsBuffer, sizeof(m_SaveAsBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

		// ファイル名に使えない文字を取り除き、asset\scene\名前.json に保存する
		std::string name;
		for (const char* p = m_SaveAsBuffer; *p; p++)
		{
			if (strchr("\\/:*?\"<>|", *p) == nullptr) name += *p;
		}
		std::string path = "asset\\scene\\" + name + ".json";

		ImGui::TextDisabled("%s", path.c_str());
		bool exists = !name.empty() && GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
		if (exists) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "同じ名前のシーンがあります。上書きされます");

		ImGui::BeginDisabled(name.empty());
		if (ImGui::Button("保存", ImVec2(120.0f, 0.0f)) || (enter && !name.empty()))
		{
			SaveScene(path);
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("キャンセル", ImVec2(120.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
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
	DrawEditMenu();

	// Unity のように中央に並べる
	float h = ImGui::GetFrameHeight();
	float groupWidth = h * 1.4f * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
	ImGui::SetCursorPosX((ImGui::GetWindowWidth() - groupWidth) * 0.5f);

	// ▶：止まっていれば再生。Play 中は押しても何もしない
	if (ToolbarIconButton("##Play", EditorIcon::Play, m_PlayState == PlayState::Play, "再生 (Ctrl+P)"))
	{
		if (m_PlayState != PlayState::Play) Play();
	}

	// ❚❚：Play 中なら一時停止、一時停止中なら再開
	if (ToolbarIconButton("##Pause", EditorIcon::Pause, m_PlayState == PlayState::Pause, "一時停止 (Ctrl+Shift+P)"))
	{
		TogglePause();
	}

	// ■：シーンを読み込み直して最初の状態に戻す（読み込み後の1フレーム更新は Manager 側で行う）
	if (ToolbarIconButton("##Stop", EditorIcon::Stop, false, "停止・最初の状態に戻す (Ctrl+P)"))
	{
		Stop();
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
	Vector3 origin, direction;
	if (!ScreenRayFromMouse(m_SceneMin, m_SceneMax, origin, direction)) return Vector3(0.0f, 0.0f, 0.0f);

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
		DrawGizmoToolbar();

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
			if (ImGui::IsKeyPressed(ImGuiKey_W, false)) m_GizmoOperation = GizmoOperation::Translate;
			if (ImGui::IsKeyPressed(ImGuiKey_E, false)) m_GizmoOperation = GizmoOperation::Rotate;
			if (ImGui::IsKeyPressed(ImGuiKey_R, false)) m_GizmoOperation = GizmoOperation::Scale;
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
	if (GizmoIconButton("##Move", EditorIcon::Move, m_GizmoOperation == GizmoOperation::Translate, "移動 (W)"))
		m_GizmoOperation = GizmoOperation::Translate;
	ImGui::SameLine();
	if (GizmoIconButton("##Rotate", EditorIcon::Rotate, m_GizmoOperation == GizmoOperation::Rotate, "回転 (E)"))
		m_GizmoOperation = GizmoOperation::Rotate;
	ImGui::SameLine();
	if (GizmoIconButton("##Scale", EditorIcon::Scale, m_GizmoOperation == GizmoOperation::Scale, "拡縮 (R)"))
		m_GizmoOperation = GizmoOperation::Scale;
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	if (GizmoIconButton("##Space", m_GizmoLocal ? EditorIcon::Local : EditorIcon::World, false,
		m_GizmoLocal ? "ローカル座標（押すとワールド座標）" : "ワールド座標（押すとローカル座標）"))
	{
		m_GizmoLocal = !m_GizmoLocal;
	}
}

//=============================================================
// 選択中のオブジェクトのギズモ（ImGuizmo）
//=============================================================
void EditorGUI::DrawTransformGizmo()
{
	GameObject* object = Manager::FindGameObjectByID(m_SelectedID);
	if (object == nullptr) return;
	if (object->GetLayer() == LAYER_2D) return;	// 2D のオブジェクトは対象外

	XMMATRIX view, projection;
	if (!GetActiveViewProjection(view, projection)) return;

	XMFLOAT4X4 viewF, projectionF, worldF;
	XMStoreFloat4x4(&viewF, view);
	XMStoreFloat4x4(&projectionF, projection);
	XMStoreFloat4x4(&worldF, object->GetWorldMatrix());

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist(m_SceneDrawList);
	ImGuizmo::SetRect(m_SceneMin[0], m_SceneMin[1], m_SceneMax[0] - m_SceneMin[0], m_SceneMax[1] - m_SceneMin[1]);

	// Ctrl を押している間はスナップ（移動 1m / 回転 15度 / 拡縮 0.1）
	ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
	float snap[3] = { 1.0f, 1.0f, 1.0f };
	switch (m_GizmoOperation)
	{
	case GizmoOperation::Translate: operation = ImGuizmo::TRANSLATE; break;
	case GizmoOperation::Rotate:    operation = ImGuizmo::ROTATE; snap[0] = 15.0f; break;
	case GizmoOperation::Scale:     operation = ImGuizmo::SCALE;  snap[0] = snap[1] = snap[2] = 0.1f; break;
	}
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

	// 回転・拡縮以外では書き換えない（誤差で数値の表記が変わるのを防ぐ）
	object->SetPosition(position);
	if (m_GizmoOperation == GizmoOperation::Rotate) object->SetRotation(RotationFromQuaternion(rotation));
	if (m_GizmoOperation == GizmoOperation::Scale)  object->SetScale(scaleValue);
}

//=============================================================
// シーンビューのクリックでオブジェクトを選ぶ
// 1. マウス位置から伸ばした光線とコライダーの箱が当たったもののうち、一番手前
// 2. どれにも当たらなければ、画面上でオブジェクトの位置に一番近いもの（25px 以内）
//=============================================================
void EditorGUI::PickObject()
{
	Vector3  origin, direction;
	XMMATRIX viewProjection;
	if (!ScreenRayFromMouse(m_SceneMin, m_SceneMax, origin, direction, &viewProjection)) return;

	ImVec2 mouse = ImGui::GetIO().MousePos;
	float width = m_SceneMax[0] - m_SceneMin[0];
	float height = m_SceneMax[1] - m_SceneMin[1];

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

			Vector3 boundsMin, boundsMax;
			ColliderBounds(collider->GetShape(), boundsMin, boundsMax);

			const float* lo = &boundsMin.x;
			const float* hi = &boundsMax.x;
			const float* o = &origin.x;
			const float* d = &direction.x;

			// スラブ法で光線と箱の交差を調べる
			float tMin = 0.0f, tMax = FLT_MAX;
			bool hit = true;
			for (int axis = 0; axis < 3; axis++)
			{
				if (fabsf(d[axis]) < 1e-6f)
				{
					if (o[axis] < lo[axis] || o[axis] > hi[axis]) { hit = false; break; }
				}
				else
				{
					float t1 = (lo[axis] - o[axis]) / d[axis];
					float t2 = (hi[axis] - o[axis]) / d[axis];
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
			if (object->IsDestroyed() || object->GetLayer() == LAYER_2D) continue;

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
// 空いている場所を右クリックしたときに作れるもの
struct CreateMenuItem
{
	const char* Label;
	GameObject* (*Create)(const Vector3&);
	bool        SeparatorBefore;	// この項目の前に区切り線を引く
	bool        MatchEditorCamera;	// エディタカメラと同じ位置・向きにする
};

static const CreateMenuItem CREATE_MENU[] =
{
	{ "空のオブジェクト", Prefabs::CreateEmpty,   false, false },
	{ "四角",             Prefabs::CreateCube,    true,  false },
	{ "球",               Prefabs::CreateSphere,  false, false },
	{ "カプセル",         Prefabs::CreateCapsule, false, false },
	{ "カメラ",           Prefabs::CreateCamera,  true,  true  },
};

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
	const CreateMenuItem* create = nullptr;
	if (ImGui::BeginPopupContextItem("##HierarchyContext"))
	{
		for (const CreateMenuItem& item : CREATE_MENU)
		{
			if (item.SeparatorBefore) ImGui::Separator();
			if (ImGui::MenuItem(item.Label)) create = &item;
		}
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

	if (create)
	{
		// エディタカメラの 10m 前に置く
		Vector3 position(0.0f, 0.0f, 0.0f);
		if (EditorCamera::IsInitialized()) position = EditorCamera::GetPosition() + EditorCamera::GetForward() * 10.0f;

		GameObject* object = create->Create(position);

		// カメラはエディタカメラにぴったり重ねる（今見ている景色がそのまま映る）
		if (object && create->MatchEditorCamera && EditorCamera::IsInitialized())
		{
			object->SetPosition(EditorCamera::GetPosition());
			Vector3 f = EditorCamera::GetForward();
			object->SetRotation({ asinf(-f.y), atan2f(f.x, f.z), 0.0f });
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

	bool renaming = (m_RenamingID == object->GetID());
	bool open = ImGui::TreeNodeEx("##node", flags, "%s", renaming ? "" : object->GetName().c_str());

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
	Component* removeComponent = nullptr;	// 回している最中に消すと壊れるので、ループの後で消す
	for (Component* component : object->GetComponents())
	{
		ImGui::PushID(index++);
		std::string label = TypeName(typeid(*component).name());
		bool open = ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

		// 見出しの右クリック、または右端の「…」ボタンでメニュー
		if (ImGui::BeginPopupContextItem("ComponentMenu"))
		{
			if (ImGui::MenuItem("コンポーネントを削除")) removeComponent = component;
			ImGui::EndPopup();
		}
		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetFrameHeight() - ImGui::GetStyle().WindowPadding.x - 4.0f);
		if (ImGui::SmallButton("...")) ImGui::OpenPopup("ComponentMenu");

		if (open)
		{
			bool enabled = component->IsEnabled();
			if (ImGui::Checkbox("Enabled", &enabled)) component->SetEnabled(enabled);

			component->OnInspectorGUI();
		}
		ImGui::PopID();
	}

	if (removeComponent) object->RemoveComponent(removeComponent);

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

		Vector3 mn, mx;
		ColliderBounds(collider->GetShape(), mn, mx);

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
