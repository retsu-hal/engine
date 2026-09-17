#include "main.h"
#include "EditorGUI.h"
#include "EditorGUIInternal.h"
#include "Manager.h"
#include "GameObject.h"
#include "Camera.h"
#include "CameraComponent.h"
#include "EditorCamera.h"
#include "Collider.h"
#include "Prefabs.h"
#include "SceneSerializer.h"
#include "UndoSystem.h"
#include "Console.h"
#include "AssetBrowser.h"
#include "GameBuilder.h"
#include "ImGuizmo.h"
#include "imgui_internal.h"	// FindWindowByName
#include <cstring>
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
bool         EditorGUI::m_DuplicateRequested = false;
unsigned int EditorGUI::m_RenamingID = 0;
char         EditorGUI::m_RenameBuffer[128] = "";
bool         EditorGUI::m_RenameFocus = false;
std::string  EditorGUI::m_RestoreSelectName;
bool         EditorGUI::m_SceneVisible = true;
bool         EditorGUI::m_GameHovered = false;
bool         EditorGUI::m_GameVisible = true;
unsigned int EditorGUI::m_DockSpaceID = 0;
bool         EditorGUI::m_ResetLayout = false;
int          EditorGUI::m_FocusWindow = 0;
bool         EditorGUI::m_ShowScene = true;
bool         EditorGUI::m_ShowGame = true;
bool         EditorGUI::m_ShowHierarchy = true;
bool         EditorGUI::m_ShowInspector = true;
bool         EditorGUI::m_ShowProject = true;
bool         EditorGUI::m_ShowConsole = true;
bool         EditorGUI::m_ShowShortcuts = false;
bool         EditorGUI::m_ShowBuildSettings = false;

//=============================================================
// 分けたファイル同士で使う小さな関数
//=============================================================
namespace EditorInternal
{

// 今シーンビューに映しているカメラの行列（止めている間はエディタカメラ、Play 中はゲームカメラ）
bool GetActiveViewProjection(XMMATRIX& view, XMMATRIX& projection)
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

// 回転行列 → GameObject の回転（XMMatrixRotationRollPitchYaw と同じ並び。x=pitch y=yaw z=roll）
Vector3 RotationFromQuaternion(FXMVECTOR quaternion)
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

// マウス位置からシーンビューの奥へ伸ばした光線
void ScreenRay(const XMMATRIX& viewProjection, Vector3& origin, Vector3& direction)
{
	float minX, minY, maxX, maxY;
	EditorGUI::GetSceneRect(&minX, &minY, &maxX, &maxY);

	// マウス位置をシーンビューの中で -1〜+1 に直す
	ImVec2 mouse = ImGui::GetIO().MousePos;
	float ndcX = (mouse.x - minX) / (maxX - minX) * 2.0f - 1.0f;
	float ndcY = 1.0f - (mouse.y - minY) / (maxY - minY) * 2.0f;

	// 手前と奥の点に戻し、その2点を結ぶ向きが光線になる
	XMMATRIX inverse = XMMatrixInverse(nullptr, viewProjection);
	XMVECTOR nearPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), inverse);
	XMVECTOR farPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), inverse);

	XMStoreFloat3((XMFLOAT3*)&origin, nearPoint);
	XMStoreFloat3((XMFLOAT3*)&direction, XMVector3Normalize(farPoint - nearPoint));
}

// asset\scene にある .json の一覧
std::vector<std::string> GetSceneFiles()
{
	std::vector<std::string> files;

	WIN32_FIND_DATAA find;
	HANDLE handle = FindFirstFileA("asset\\scene\\*.json", &find);
	if (handle != INVALID_HANDLE_VALUE)
	{
		do
		{
			files.push_back(std::string("asset\\scene\\") + find.cFileName);
		} while (FindNextFileA(handle, &find));
		FindClose(handle);
	}
	return files;
}

}	// namespace EditorInternal

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

	BuildDefaultLayout();

	DrawToolbar();

	m_SceneVisible = false;
	m_SceneHovered = false;
	m_SceneDrawList = nullptr;
	if (m_ShowScene) DrawSceneView();

	m_GameVisible = false;
	m_GameHovered = false;
	if (m_ShowGame) DrawGameView();

	if (m_ShowHierarchy) DrawHierarchy();
	if (m_ShowInspector) DrawInspector();
	if (m_ShowProject)   AssetBrowser::Draw(&m_ShowProject);
	if (m_ShowConsole)   Console::Draw(&m_ShowConsole);
	DrawShortcutsWindow();
	DrawBuildSettings();
	GameBuilder::Update();

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
	if (m_PlayState == PlayState::Edit)
	{
		Console::OnPlay();
		m_FocusWindow = 2;	// Game タブを前に出す
	}
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
	m_FocusWindow = 1;	// Scene タブを前に出す
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
		if (editing && (undo || redo)) UndoRedo(redo);

		// Ctrl+B：ビルドして実行、Ctrl+Shift+B：ビルド設定
		if (ImGui::IsKeyPressed(ImGuiKey_B, false))
		{
			if (io.KeyShift) m_ShowBuildSettings = true;
			else if (GameBuilder::GetState() != GameBuilder::State::Building) GameBuilder::Start(true);
		}

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

	// Alt+Shift+A：アクティブ切り替え（Unity と同じ）
	if (allowKeys && io.KeyAlt && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_A, false))
	{
		UndoSystem::Begin();
		selected->SetActive(!selected->IsActiveSelf());
	}
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
// 初回だけ Unity に近い配置を作る（imgui.ini に配置が保存されていればそちらを使う）
//   ┌──────────┬───────────┬────────────┬──────────────┐
//   │Inspector │ Hierarchy │  Project   │    Scene     │
//   │          ├───────────┴────────────┼──────────────┤
//   │          │        Console         │    Game      │
//   └──────────┴────────────────────────┴──────────────┘
//=============================================================
void EditorGUI::BuildDefaultLayout()
{
	if (m_DockSpaceID == 0) return;

	// 起動して最初のフレームで1回だけ調べる（自分で配置を変えたあとに勝手に戻さないため）
	static bool firstFrame = true;
	if (!firstFrame && !m_ResetLayout) return;
	firstFrame = false;

	ImGuiDockNode* node = ImGui::DockBuilderGetNode(m_DockSpaceID);
	bool alreadyArranged = (node != nullptr && node->IsSplitNode());
	if (alreadyArranged && !m_ResetLayout) return;
	m_ResetLayout = false;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::DockBuilderRemoveNodeChildNodes(m_DockSpaceID);
	ImGui::DockBuilderSetNodeSize(m_DockSpaceID, viewport->WorkSize);

	ImGuiID rest = m_DockSpaceID;
	ImGuiID left   = ImGui::DockBuilderSplitNode(rest, ImGuiDir_Left, 0.20f, nullptr, &rest);
	ImGuiID right  = ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.52f, nullptr, &rest);
	ImGuiID bottom = ImGui::DockBuilderSplitNode(rest, ImGuiDir_Down, 0.45f, nullptr, &rest);
	ImGuiID project = ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.55f, nullptr, &rest);
	ImGuiID game   = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.45f, nullptr, &right);

	ImGui::DockBuilderDockWindow("Inspector", left);
	ImGui::DockBuilderDockWindow("Hierarchy", rest);
	ImGui::DockBuilderDockWindow("Project", project);
	ImGui::DockBuilderDockWindow("Console", bottom);
	ImGui::DockBuilderDockWindow("Scene", right);
	ImGui::DockBuilderDockWindow("Game", game);
	ImGui::DockBuilderFinish(m_DockSpaceID);
}

//=============================================================
// オブジェクトの作成（Hierarchy の右クリックと GameObject メニューで共通）
//=============================================================
GameObject* EditorGUI::CreateObject(ObjectType type)
{
	if (type == ObjectType::None) return nullptr;

	UndoSystem::Begin();

	// エディタカメラの 10m 前に置く
	Vector3 position(0.0f, 0.0f, 0.0f);
	if (EditorCamera::IsInitialized()) position = EditorCamera::GetPosition() + EditorCamera::GetForward() * 10.0f;

	GameObject* object = nullptr;
	switch (type)
	{
	case ObjectType::Empty:   object = Prefabs::CreateEmpty(position);   break;
	case ObjectType::Cube:    object = Prefabs::CreateCube(position);    break;
	case ObjectType::Sphere:  object = Prefabs::CreateSphere(position);  break;
	case ObjectType::Capsule: object = Prefabs::CreateCapsule(position); break;
	case ObjectType::Camera:
		object = Prefabs::CreateCamera(position);
		// エディタカメラと同じ位置・向きにしておく（今見ている景色がそのまま映る）
		if (object && EditorCamera::IsInitialized())
		{
			object->SetPosition(EditorCamera::GetPosition());
			Vector3 f = EditorCamera::GetForward();
			object->SetRotation({ asinf(-f.y), atan2f(f.x, f.z), 0.0f });
		}
		break;

	default:
		break;
	}
	return object;
}

// 作って、それを選択中にする
void EditorGUI::CreateAndSelect(ObjectType type)
{
	if (GameObject* object = CreateObject(type)) m_SelectedID = object->GetID();
}

void EditorGUI::NewScene()
{
	m_PlayState = PlayState::Edit;
	m_HasSnapshot = false;
	m_SelectedID = 0;
	m_ScenePath.clear();	// 名前がないので、保存のときに名前を聞く
	UndoSystem::Clear();
	Manager::LoadSceneText("{\"version\":1,\"objects\":[{\"id\":1,\"class\":\"GameObject\",\"name\":\"Main Camera\",\"parent\":0,\"layer\":1,"
		"\"position\":[0,1,-10],\"rotation\":[0,0,0],\"scale\":[1,1,1],\"components\":[{\"type\":\"CameraComponent\",\"enabled\":true}]}]}");
}

//=============================================================
// シーンの保存
//=============================================================
void EditorGUI::SaveScene(const std::string& path)
{
	CreateDirectoryA("asset\\scene", nullptr);	// なければ作る（あれば何もしない）

	int skipped = SceneSerializer::SaveToFile(path);
	if (skipped >= 0) m_ScenePath = path;
	if (skipped > 0)
	{
		Debug::LogWarning("登録されていないオブジェクト %d 個は保存されていません", skipped);
	}
	else if (skipped == 0)
	{
		Debug::Log("シーンを保存しました: %s", path.c_str());
	}
}

// 上書き保存する。まだ名前がないシーンは「名前を付けて保存」を開く
void EditorGUI::SaveCurrentScene()
{
	if (m_ScenePath.empty())
	{
		strncpy_s(m_SaveAsBuffer, "NewScene", _TRUNCATE);
		m_OpenSaveAsPopup = true;
	}
	else
	{
		SaveScene(m_ScenePath);
	}
}

//=============================================================
// 元に戻す／やり直し
// 読み込み直しで選択中のオブジェクトが別物になるので、あとで同じ名前のものを選び直す
//=============================================================
void EditorGUI::UndoRedo(bool redo)
{
	if (GameObject* object = Manager::FindGameObjectByID(m_SelectedID)) m_RestoreSelectName = object->GetName();

	bool done = redo ? UndoSystem::Redo() : UndoSystem::Undo();
	if (done) m_SelectedID = 0;
	else      m_RestoreSelectName.clear();
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
		Vector3 half = shape.GetHalfExtents();
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
