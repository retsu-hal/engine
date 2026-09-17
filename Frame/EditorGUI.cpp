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
#include "Console.h"
#include "SceneGrid.h"
#include "Gizmo.h"
#include "ScriptTool.h"
#include "GameBuilder.h"
#include "ProjectSettings.h"
#include "MeshField.h"
#include "imgui_internal.h"	// BeginDragDropTargetCustom
#include "JsonUtil.h"
#include <typeinfo>
#include <cstring>
#include <cfloat>
#include <algorithm>
#include <cctype>

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
// 編集メニュー
//=============================================================
void EditorGUI::DrawEditMenu()
{
	if (!ImGui::BeginMenu("Edit")) return;

	bool editing = (m_PlayState != PlayState::Play);
	bool selected = (Manager::FindGameObjectByID(m_SelectedID) != nullptr);

	if (ImGui::MenuItem("元に戻す", "Ctrl+Z", false, editing && UndoSystem::CanUndo()))
	{
		if (GameObject* object = Manager::FindGameObjectByID(m_SelectedID)) m_RestoreSelectName = object->GetName();
		UndoSystem::Undo();
		m_SelectedID = 0;
	}
	if (ImGui::MenuItem("やり直し", "Ctrl+Y", false, editing && UndoSystem::CanRedo()))
	{
		if (GameObject* object = Manager::FindGameObjectByID(m_SelectedID)) m_RestoreSelectName = object->GetName();
		UndoSystem::Redo();
		m_SelectedID = 0;
	}
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
		if (editing && (undo || redo))
		{
			if (GameObject* object = Manager::FindGameObjectByID(m_SelectedID)) m_RestoreSelectName = object->GetName();
			bool done = undo ? UndoSystem::Undo() : UndoSystem::Redo();
			if (done) m_SelectedID = 0;
			else m_RestoreSelectName.clear();
		}

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
GameObject* EditorGUI::CreateObject(int type)
{
	UndoSystem::Begin();

	// エディタカメラの 10m 前に置く
	Vector3 position(0.0f, 0.0f, 0.0f);
	if (EditorCamera::IsInitialized()) position = EditorCamera::GetPosition() + EditorCamera::GetForward() * 10.0f;

	GameObject* object = nullptr;
	switch (type)
	{
	case 0: object = Prefabs::CreateEmpty(position);   break;
	case 1: object = Prefabs::CreateCube(position);    break;
	case 2: object = Prefabs::CreateSphere(position);  break;
	case 3: object = Prefabs::CreateCapsule(position); break;
	case 4:
		object = Prefabs::CreateCamera(position);
		// エディタカメラと同じ位置・向きにしておく（今見ている景色がそのまま映る）
		if (object && EditorCamera::IsInitialized())
		{
			object->SetPosition(EditorCamera::GetPosition());
			Vector3 f = EditorCamera::GetForward();
			object->SetRotation({ asinf(-f.y), atan2f(f.x, f.z), 0.0f });
		}
		break;
	}
	return object;
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
// Assets / GameObject / Component / Window / Help メニュー
//=============================================================
void EditorGUI::DrawAssetsMenu()
{
	if (!ImGui::BeginMenu("Assets")) return;

	if (ImGui::BeginMenu("作成"))
	{
		AssetBrowser::DrawCreateMenu();
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("エクスプローラーで表示")) AssetBrowser::ShowInExplorer(AssetBrowser::GetCurrentFolder());
	ImGui::Separator();
	if (ImGui::MenuItem("更新", "Ctrl+R")) AssetBrowser::RequestRefresh();

	ImGui::EndMenu();
}

void EditorGUI::DrawGameObjectMenu()
{
	if (!ImGui::BeginMenu("GameObject")) return;

	int createType = -1;
	if (ImGui::MenuItem("空のオブジェクト")) createType = 0;
	if (ImGui::BeginMenu("3D オブジェクト"))
	{
		if (ImGui::MenuItem("四角"))     createType = 1;
		if (ImGui::MenuItem("球"))       createType = 2;
		if (ImGui::MenuItem("カプセル")) createType = 3;
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("カメラ")) createType = 4;

	GameObject* selected = Manager::FindGameObjectByID(m_SelectedID);
	ImGui::Separator();
	if (ImGui::MenuItem("選択中のものの子として空のオブジェクトを作成", nullptr, false, selected != nullptr))
	{
		if (GameObject* child = CreateObject(0))
		{
			child->SetParent(selected);
			child->SetPosition({ 0.0f, 0.0f, 0.0f });
			m_SelectedID = child->GetID();
		}
	}

	if (createType >= 0)
	{
		if (GameObject* object = CreateObject(createType)) m_SelectedID = object->GetID();
	}

	ImGui::EndMenu();
}

void EditorGUI::DrawComponentMenu()
{
	if (!ImGui::BeginMenu("Component")) return;

	GameObject* selected = Manager::FindGameObjectByID(m_SelectedID);
	if (selected == nullptr) ImGui::TextDisabled("オブジェクトを選択してください");

	for (auto& pair : ComponentRegistry::GetAll())
	{
		if (ImGui::MenuItem(pair.first.c_str(), nullptr, false, selected != nullptr))
		{
			UndoSystem::Begin();
			selected->AddComponentInstance(pair.second(selected));
		}
	}

	ImGui::EndMenu();
}

void EditorGUI::DrawWindowMenu()
{
	if (!ImGui::BeginMenu("Window")) return;

	ImGui::MenuItem("Scene", nullptr, &m_ShowScene);
	ImGui::MenuItem("Game", nullptr, &m_ShowGame);
	ImGui::MenuItem("Hierarchy", nullptr, &m_ShowHierarchy);
	ImGui::MenuItem("Inspector", nullptr, &m_ShowInspector);
	ImGui::MenuItem("Project", nullptr, &m_ShowProject);
	ImGui::MenuItem("Console", nullptr, &m_ShowConsole);
	ImGui::Separator();
	ImGui::MenuItem("グリッド表示", nullptr, SceneGrid::GetEnablePtr());
	bool gizmo = Gizmo::IsEnable();
	if (ImGui::MenuItem("ギズモ表示", "F1", &gizmo)) Gizmo::SetEnable(gizmo);
	ImGui::Separator();
	if (ImGui::MenuItem("レイアウトを初期状態に戻す"))
	{
		m_ShowScene = m_ShowGame = m_ShowHierarchy = m_ShowInspector = m_ShowProject = m_ShowConsole = true;
		m_ResetLayout = true;
	}

	ImGui::EndMenu();
}

void EditorGUI::DrawHelpMenu()
{
	if (!ImGui::BeginMenu("Help")) return;
	ImGui::MenuItem("ショートカット一覧", nullptr, &m_ShowShortcuts);
	ImGui::Separator();
	ImGui::TextDisabled("GM31 Engine  (DirectX11 / Dear ImGui %s)", IMGUI_VERSION);
	ImGui::EndMenu();
}

void EditorGUI::DrawShortcutsWindow()
{
	if (!m_ShowShortcuts) return;

	ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("ショートカット一覧", &m_ShowShortcuts, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static const char* list[][2] = {
			{ "Ctrl+S", "シーンを保存" },
			{ "Ctrl+Z / Ctrl+Y", "元に戻す / やり直し" },
			{ "Ctrl+D", "複製" },
			{ "Delete", "削除" },
			{ "F2", "名前の変更" },
			{ "F", "選択中のオブジェクトに寄る" },
			{ "W / E / R", "移動 / 回転 / 拡縮" },
			{ "Ctrl+P", "再生・停止" },
			{ "Ctrl+Shift+P", "一時停止" },
			{ "Ctrl+B", "ビルドして実行" },
			{ "Ctrl+Shift+B", "ビルド設定" },
			{ "F1", "ギズモ表示" },
			{ "右ドラッグ + WASD/QE", "Scene のカメラ移動" },
			{ "Ctrl+ホイール", "Project のアイコンの大きさ" },
			{ "Ctrl+R", "Project を更新" },
			{ "Alt+Ctrl+C", "アセットのパスをコピー" },
		};
		if (ImGui::BeginTable("##shortcuts", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
		{
			for (auto& row : list)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(row[0]);
				ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(row[1]);
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

//=============================================================
// ビルド設定（ゲーム用 exe の作成）
//=============================================================
void EditorGUI::DrawBuildSettings()
{
	// ビルド中は、閉じていても右下に小さく状況を出す
	if (GameBuilder::GetState() == GameBuilder::State::Building && !m_ShowBuildSettings)
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 16.0f, viewport->WorkPos.y + viewport->WorkSize.y - 16.0f), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
		ImGui::SetNextWindowBgAlpha(0.85f);
		if (ImGui::Begin("##BuildProgress", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
		{
			ImGui::Text("ビルド中... %.0f 秒", GameBuilder::GetElapsedSeconds());
		}
		ImGui::End();
	}

	if (!m_ShowBuildSettings) return;

	ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("ビルド設定", &m_ShowBuildSettings, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	// タイトル
	char title[128];
	strncpy_s(title, ProjectSettings::Title.c_str(), _TRUNCATE);
	if (ImGui::InputText("ゲームの名前", title, sizeof(title))) ProjectSettings::Title = title;

	// 最初のシーン（asset\scene の中から選ぶ）
	if (ImGui::BeginCombo("最初のシーン", ProjectSettings::StartScene.c_str()))
	{
		WIN32_FIND_DATAA find;
		HANDLE handle = FindFirstFileA("asset\\scene\\*.json", &find);
		if (handle != INVALID_HANDLE_VALUE)
		{
			do
			{
				std::string path = std::string("asset\\scene\\") + find.cFileName;
				if (ImGui::Selectable(path.c_str(), path == ProjectSettings::StartScene)) ProjectSettings::StartScene = path;
			} while (FindNextFileA(handle, &find));
			FindClose(handle);
		}
		ImGui::EndCombo();
	}
	if (!m_ScenePath.empty() && ImGui::SmallButton("今開いているシーンにする")) ProjectSettings::StartScene = m_ScenePath;

	ImGui::TextDisabled("出力先: %s\\（exe・asset・shader・DLL をまとめます）", GameBuilder::GetOutputFolder().c_str());
	ImGui::Separator();

	bool building = (GameBuilder::GetState() == GameBuilder::State::Building);
	ImGui::BeginDisabled(building);
	if (ImGui::Button("設定を保存", ImVec2(110.0f, 0.0f))) ProjectSettings::Save();
	ImGui::SameLine();
	if (ImGui::Button("ビルド", ImVec2(110.0f, 0.0f))) GameBuilder::Start(false);
	ImGui::SameLine();
	if (ImGui::Button("ビルドして実行", ImVec2(130.0f, 0.0f))) GameBuilder::Start(true);
	ImGui::EndDisabled();

	switch (GameBuilder::GetState())
	{
	case GameBuilder::State::Building:
		ImGui::Text("ビルド中... %.0f 秒（Console に経過が出ます）", GameBuilder::GetElapsedSeconds());
		break;
	case GameBuilder::State::Succeeded:
		ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "完了しました");
		ImGui::SameLine();
		if (ImGui::SmallButton("フォルダを開く")) GameBuilder::OpenOutputFolder();
		break;
	case GameBuilder::State::Failed:
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "失敗しました（Console を確認してください）");
		break;
	default:
		break;
	}

	ImGui::End();
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
		Debug::LogWarning("登録されていないオブジェクト %d 個は保存されていません", skipped);
	}
	else if (skipped == 0)
	{
		Debug::Log("シーンを保存しました: %s", path.c_str());
	}
}

void EditorGUI::DrawFileMenu()
{
	ImGuiIO& io = ImGui::GetIO();

	// Ctrl+S で上書き保存（Play 中は保存しない。動いている途中の状態が残ってしまうため）
	bool canSave = (m_PlayState == PlayState::Edit);
	if (canSave && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) && !io.WantTextInput)
	{
		// まだ名前がないシーンは「名前を付けて保存」
		if (m_ScenePath.empty()) { strncpy_s(m_SaveAsBuffer, "NewScene", _TRUNCATE); m_OpenSaveAsPopup = true; }
		else SaveScene(m_ScenePath);
	}

	if (ImGui::BeginMenu("File"))
	{
		ImGui::TextDisabled("%s", m_ScenePath.empty() ? "(保存していないシーン)" : m_ScenePath.c_str());
		ImGui::Separator();

		if (ImGui::MenuItem("新規シーン", nullptr, false, canSave)) NewScene();
		ImGui::Separator();

		if (ImGui::MenuItem("シーンを保存", "Ctrl+S", false, canSave))
		{
			if (m_ScenePath.empty()) { strncpy_s(m_SaveAsBuffer, "NewScene", _TRUNCATE); m_OpenSaveAsPopup = true; }
			else SaveScene(m_ScenePath);
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

		ImGui::Separator();
		ImGui::MenuItem("ビルド設定...", "Ctrl+Shift+B", &m_ShowBuildSettings);
		if (ImGui::MenuItem("ビルドして実行", "Ctrl+B", false, GameBuilder::GetState() != GameBuilder::State::Building)) GameBuilder::Start(true);

		ImGui::Separator();
		if (ImGui::MenuItem("終了")) PostMessage(GetWindow(), WM_CLOSE, 0, 0);

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
	DrawAssetsMenu();
	DrawGameObjectMenu();
	DrawComponentMenu();
	DrawWindowMenu();
	DrawHelpMenu();

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
	if (iconButton("##Play", Icon::Play, m_PlayState == PlayState::Play, "再生 (Ctrl+P)"))
	{
		if (m_PlayState != PlayState::Play) Play();
	}

	// ❚❚：Play 中なら一時停止、一時停止中なら再開
	if (iconButton("##Pause", Icon::Pause, m_PlayState == PlayState::Pause, "一時停止 (Ctrl+Shift+P)"))
	{
		TogglePause();
	}

	// ■：シーンを読み込み直して最初の状態に戻す（読み込み後の1フレーム更新は Manager 側で行う）
	if (iconButton("##Stop", Icon::Stop, false, "停止・最初の状態に戻す (Ctrl+P)"))
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
		float aspect = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;
		ImVec2 size(avail.x, avail.x / aspect);
		if (size.y > avail.y) size = ImVec2(avail.y * aspect, avail.y);
		if (size.x < 1.0f || size.y < 1.0f) size = ImVec2(1.0f, 1.0f);

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
// ギズモ切り替えのアイコンボタン（移動・回転・拡縮・ローカル/ワールド）
//=============================================================
void EditorGUI::DrawGizmoToolbar()
{
	enum class Icon { Move, Rotate, Scale, Local, World };

	auto iconButton = [](const char* id, Icon icon, bool active, const char* tooltip) -> bool
	{
		float h = ImGui::GetFrameHeight();
		ImVec2 size(h * 1.3f, h);

		if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		bool pressed = ImGui::Button(id, size);
		if (active) ImGui::PopStyleColor();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);

		ImVec2 min = ImGui::GetItemRectMin();
		ImVec2 max = ImGui::GetItemRectMax();
		ImVec2 c((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
		float r = h * 0.32f;
		ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float t = 1.5f;	// 線の太さ

		switch (icon)
		{
		case Icon::Move:	// 十字の矢印
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
		case Icon::Rotate:	// 回る矢印
		{
			dl->PathArcTo(c, r * 0.85f, XM_PI * 0.15f, XM_PI * 1.75f, 20);
			dl->PathStroke(color, 0, t);
			ImVec2 tip(c.x + cosf(XM_PI * 1.75f) * r * 0.85f, c.y + sinf(XM_PI * 1.75f) * r * 0.85f);
			float a = r * 0.4f;
			dl->AddTriangleFilled(ImVec2(tip.x + a, tip.y), ImVec2(tip.x - a * 0.3f, tip.y - a), ImVec2(tip.x - a * 0.3f, tip.y + a * 0.6f), color);
			break;
		}
		case Icon::Scale:	// 小さい四角から大きい四角へ伸びる
		{
			dl->AddRect(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), color, 0.0f, 0, t);
			dl->AddRectFilled(ImVec2(c.x - r, c.y + r * 0.1f), ImVec2(c.x - r * 0.1f, c.y + r), color);
			dl->AddLine(ImVec2(c.x - r * 0.2f, c.y + r * 0.2f), ImVec2(c.x + r * 0.7f, c.y - r * 0.7f), color, t);
			float a = r * 0.35f;
			dl->AddTriangleFilled(ImVec2(c.x + r * 0.85f, c.y - r * 0.85f), ImVec2(c.x + r * 0.85f - a * 1.4f, c.y - r * 0.85f), ImVec2(c.x + r * 0.85f, c.y - r * 0.85f + a * 1.4f), color);
			break;
		}
		case Icon::Local:	// 立方体（自分の向き）
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
		case Icon::World:	// 地球（円と経線・緯線）
		{
			dl->AddCircle(c, r, color, 20, t);
			dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), color, t);
			dl->AddEllipse(c, ImVec2(r * 0.45f, r), color, 0.0f, 20, t);
			break;
		}
		}
		return pressed;
	};

	if (iconButton("##Move", Icon::Move, m_GizmoOperation == 0, "移動 (W)"))   m_GizmoOperation = 0;
	ImGui::SameLine();
	if (iconButton("##Rotate", Icon::Rotate, m_GizmoOperation == 1, "回転 (E)")) m_GizmoOperation = 1;
	ImGui::SameLine();
	if (iconButton("##Scale", Icon::Scale, m_GizmoOperation == 2, "拡縮 (R)"))  m_GizmoOperation = 2;
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	if (iconButton("##Space", m_GizmoLocal ? Icon::Local : Icon::World, false,
		m_GizmoLocal ? "ローカル座標（押すとワールド座標）" : "ワールド座標（押すとローカル座標）"))
	{
		m_GizmoLocal = !m_GizmoLocal;
	}

	// グリッドの表示切り替え（井桁のアイコン）
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	{
		float h = ImGui::GetFrameHeight();
		bool on = SceneGrid::IsEnable();
		if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		if (ImGui::Button("##Grid", ImVec2(h * 1.3f, h))) SceneGrid::SetEnable(!on);
		if (on) ImGui::PopStyleColor();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("グリッド");
		ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
		ImVec2 c((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
		float r = h * 0.3f;
		ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
		ImDrawList* dl = ImGui::GetWindowDrawList();
		for (int i = -1; i <= 1; i += 2)
		{
			dl->AddLine(ImVec2(c.x + r * 0.4f * i, c.y - r), ImVec2(c.x + r * 0.4f * i, c.y + r), color, 1.5f);
			dl->AddLine(ImVec2(c.x - r, c.y + r * 0.4f * i), ImVec2(c.x + r, c.y + r * 0.4f * i), color, 1.5f);
		}
	}

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
		if (object->IsDestroyed() || !object->IsActiveInHierarchy()) continue;

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
	int createType = -1;	// 0:空 1:四角 2:球 3:カプセル 4:カメラ
	if (ImGui::BeginPopupContextItem("##HierarchyContext"))
	{
		if (ImGui::MenuItem("空のオブジェクト")) createType = 0;
		ImGui::Separator();
		if (ImGui::MenuItem("四角"))       createType = 1;
		if (ImGui::MenuItem("球"))         createType = 2;
		if (ImGui::MenuItem("カプセル"))   createType = 3;
		ImGui::Separator();
		if (ImGui::MenuItem("カメラ"))     createType = 4;
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
		if (GameObject* object = CreateObject(createType)) m_SelectedID = object->GetID();
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

//=============================================================
// Inspector 用の小さな部品
//=============================================================

// "BoxCollider" → "Box Collider"（Unity の ObjectNames.NicifyVariableName と同じ考え方）
static std::string NicifyName(const std::string& name)
{
	std::string result;
	for (size_t i = 0; i < name.size(); i++)
	{
		char c = name[i];
		if (i > 0 && isupper((unsigned char)c))
		{
			char prev = name[i - 1];
			bool nextLower = (i + 1 < name.size()) && islower((unsigned char)name[i + 1]);
			// 小文字→大文字、または "UIText" の "T" のように略語の終わり
			if (islower((unsigned char)prev) || isdigit((unsigned char)prev) || (isupper((unsigned char)prev) && nextLower))
				result += ' ';
		}
		if (c == '_') { result += ' '; continue; }
		result += c;
	}
	return result;
}

// 種類ごとのアイコン（1文字）と色
static void GetComponentIcon(const std::string& typeName, const char*& icon, ImVec4& color)
{
	auto has = [&](const char* word) { return typeName.find(word) != std::string::npos; };
	if (has("Collider"))       { icon = "C"; color = ImVec4(0.35f, 0.85f, 0.40f, 1.0f); }
	else if (has("Rigidbody")) { icon = "R"; color = ImVec4(0.30f, 0.75f, 0.95f, 1.0f); }
	else if (has("Camera"))    { icon = "Ca"; color = ImVec4(0.70f, 0.55f, 0.95f, 1.0f); }
	else if (has("Audio"))     { icon = "A"; color = ImVec4(0.95f, 0.65f, 0.25f, 1.0f); }
	else if (has("Animation")) { icon = "An"; color = ImVec4(0.95f, 0.45f, 0.60f, 1.0f); }
	else if (has("Renderer") || has("Model")) { icon = "M"; color = ImVec4(0.40f, 0.60f, 1.00f, 1.0f); }
	else if (has("Light"))     { icon = "L"; color = ImVec4(1.00f, 0.90f, 0.35f, 1.0f); }
}

// CollapsingHeader の直後に呼ぶ：見出しの上にアイコン・有効チェック・名前を重ねて描き、右端に「…」ボタンを置く位置へ移動する
static void ComponentHeaderLabel(const char* label, const char* icon, const ImVec4& iconColor, bool* enabled)
{
	ImVec2 min = ImGui::GetItemRectMin();
	ImVec2 max = ImGui::GetItemRectMax();
	float height = max.y - min.y;
	ImDrawList* draw = ImGui::GetWindowDrawList();

	// アイコン（色付きの角丸四角＋文字）
	float x = min.x + ImGui::GetTreeNodeToLabelSpacing();
	float size = height - 6.0f;
	ImVec2 iconMin(x, min.y + 3.0f);
	ImVec2 iconMax(x + size, min.y + 3.0f + size);
	draw->AddRectFilled(iconMin, iconMax, ImGui::GetColorU32(iconColor), 3.0f);
	ImVec2 textSize = ImGui::CalcTextSize(icon);
	draw->AddText(ImVec2(iconMin.x + (size - textSize.x) * 0.5f, iconMin.y + (size - textSize.y) * 0.5f), IM_COL32(20, 20, 20, 255), icon);
	x += size + 6.0f;

	// 有効チェック（見出しの上に重ねて置く）
	if (enabled)
	{
		ImGui::SameLine(x - ImGui::GetWindowPos().x + ImGui::GetScrollX());
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
		ImGui::Checkbox("##Enabled", enabled);
		ImGui::PopStyleVar();
		x = ImGui::GetItemRectMax().x + 6.0f;
	}

	// 名前（無効なら薄く）
	ImU32 textColor = ImGui::GetColorU32((enabled && !*enabled) ? ImGuiCol_TextDisabled : ImGuiCol_Text);
	draw->AddText(ImVec2(x, min.y + (height - ImGui::GetFontSize()) * 0.5f), textColor, label);

	// 右端の「…」ボタンの位置
	float buttonWidth = ImGui::CalcTextSize("...").x + ImGui::GetStyle().FramePadding.x * 2.0f;
	ImGui::SameLine(max.x - ImGui::GetWindowPos().x + ImGui::GetScrollX() - buttonWidth - 4.0f);
}

// Unity 風の Vector3 の行：「Position   [X ___] [Y ___] [Z ___]」
// X/Y/Z の色付きラベルを左右にドラッグしても値が変わる。右クリックで 0（Scale は 1）に戻す
static bool Vector3Field(const char* label, Vector3& value, float speed, float labelWidth)
{
	bool changed = false;
	ImGui::PushID(label);

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::SameLine(labelWidth);

	float spacing = ImGui::GetStyle().ItemSpacing.x;
	float letterWidth = ImGui::GetFrameHeight();
	float fieldWidth = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f - letterWidth;
	if (fieldWidth < 20.0f) fieldWidth = 20.0f;

	const char* letters[] = { "X", "Y", "Z" };
	const ImVec4 colors[] = {
		ImVec4(0.80f, 0.25f, 0.25f, 1.0f),
		ImVec4(0.30f, 0.65f, 0.25f, 1.0f),
		ImVec4(0.25f, 0.45f, 0.85f, 1.0f) };
	float* values[] = { &value.x, &value.y, &value.z };
	float resetValue = (strcmp(label, "Scale") == 0) ? 1.0f : 0.0f;

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y));
	for (int i = 0; i < 3; i++)
	{
		ImGui::PushID(i);
		if (i > 0) ImGui::SameLine(0.0f, spacing);

		ImGui::PushStyleColor(ImGuiCol_Button, colors[i]);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(colors[i].x * 1.2f, colors[i].y * 1.2f, colors[i].z * 1.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors[i]);
		ImGui::Button(letters[i], ImVec2(letterWidth, 0.0f));
		ImGui::PopStyleColor(3);

		if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		if (ImGui::IsItemActive() && ImGui::GetIO().MouseDelta.x != 0.0f)
		{
			*values[i] += ImGui::GetIO().MouseDelta.x * speed;
			changed = true;
		}
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
		{
			*values[i] = resetValue;
			changed = true;
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(fieldWidth);
		if (ImGui::DragFloat("##v", values[i], speed, 0.0f, 0.0f, "%.3f")) changed = true;
		ImGui::PopID();
	}
	ImGui::PopStyleVar();

	ImGui::PopID();
	return changed;
}

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
	ComponentHeaderLabel("Transform", "T", ImVec4(0.55f, 0.55f, 0.6f, 1.0f), nullptr);
	if (ImGui::SmallButton("...##Transform")) ImGui::OpenPopup("TransformMenu");

	if (transformOpen)
	{
		Vector3 pos = object->GetPosition();
		if (Vector3Field("Position", pos, 0.05f, labelWidth)) object->SetPosition(pos);

		// 内部はラジアン、表示は度
		Vector3 rot = object->GetRotation();
		Vector3 deg(XMConvertToDegrees(rot.x), XMConvertToDegrees(rot.y), XMConvertToDegrees(rot.z));
		if (Vector3Field("Rotation", deg, 0.5f, labelWidth))
		{
			object->SetRotation({ XMConvertToRadians(deg.x), XMConvertToRadians(deg.y), XMConvertToRadians(deg.z) });
		}

		Vector3 scale = object->GetScale();
		if (Vector3Field("Scale", scale, 0.01f, labelWidth)) object->SetScale(scale);
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
		GetComponentIcon(typeName, icon, iconColor);

		bool enabled = component->IsEnabled();
		ComponentHeaderLabel(NicifyName(typeName).c_str(), icon, iconColor, &enabled);
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
