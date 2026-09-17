#include "main.h"
#include "EditorGUI.h"
#include "EditorGUIInternal.h"
#include "EditorUI.h"
#include "Manager.h"
#include "GameObject.h"
#include "AssetBrowser.h"
#include "Registry.h"
#include "UndoSystem.h"
#include "SceneGrid.h"
#include "Gizmo.h"
#include "GameBuilder.h"
#include "ProjectSettings.h"

//=============================================================
// 編集メニュー
//=============================================================
void EditorGUI::DrawEditMenu()
{
	if (!ImGui::BeginMenu("Edit")) return;

	bool editing = (m_PlayState != PlayState::Play);
	bool selected = (Manager::FindGameObjectByID(m_SelectedID) != nullptr);

	if (ImGui::MenuItem("元に戻す", "Ctrl+Z", false, editing && UndoSystem::CanUndo())) UndoRedo(false);
	if (ImGui::MenuItem("やり直し", "Ctrl+Y", false, editing && UndoSystem::CanRedo())) UndoRedo(true);
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

//=============================================================
// 「空のオブジェクト / 四角 / 球 / カプセル / カメラ」の項目
// GameObject メニューは 3D の3つを submenu にまとめ、Hierarchy の右クリックは区切り線で並べる
//=============================================================
ObjectType EditorGUI::DrawCreateObjectMenuItems(bool use3DSubMenu)
{
	ObjectType type = ObjectType::None;

	if (ImGui::MenuItem("空のオブジェクト")) type = ObjectType::Empty;

	if (use3DSubMenu)
	{
		if (ImGui::BeginMenu("3D オブジェクト"))
		{
			if (ImGui::MenuItem("四角"))     type = ObjectType::Cube;
			if (ImGui::MenuItem("球"))       type = ObjectType::Sphere;
			if (ImGui::MenuItem("カプセル")) type = ObjectType::Capsule;
			ImGui::EndMenu();
		}
	}
	else
	{
		ImGui::Separator();
		if (ImGui::MenuItem("四角"))     type = ObjectType::Cube;
		if (ImGui::MenuItem("球"))       type = ObjectType::Sphere;
		if (ImGui::MenuItem("カプセル")) type = ObjectType::Capsule;
		ImGui::Separator();
	}

	if (ImGui::MenuItem("カメラ")) type = ObjectType::Camera;

	return type;
}

void EditorGUI::DrawGameObjectMenu()
{
	if (!ImGui::BeginMenu("GameObject")) return;

	ObjectType createType = DrawCreateObjectMenuItems(true);

	GameObject* selected = Manager::FindGameObjectByID(m_SelectedID);
	ImGui::Separator();
	if (ImGui::MenuItem("選択中のものの子として空のオブジェクトを作成", nullptr, false, selected != nullptr))
	{
		if (GameObject* child = CreateObject(ObjectType::Empty))
		{
			child->SetParent(selected);
			child->SetPosition({ 0.0f, 0.0f, 0.0f });
			m_SelectedID = child->GetID();
		}
	}

	CreateAndSelect(createType);

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
		for (const std::string& path : EditorInternal::GetSceneFiles())
		{
			if (ImGui::Selectable(path.c_str(), path == ProjectSettings::StartScene)) ProjectSettings::StartScene = path;
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
void EditorGUI::DrawFileMenu()
{
	ImGuiIO& io = ImGui::GetIO();

	// Ctrl+S で上書き保存（Play 中は保存しない。動いている途中の状態が残ってしまうため）
	bool canSave = (m_PlayState == PlayState::Edit);
	if (canSave && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) && !io.WantTextInput) SaveCurrentScene();

	if (ImGui::BeginMenu("File"))
	{
		ImGui::TextDisabled("%s", m_ScenePath.empty() ? "(保存していないシーン)" : m_ScenePath.c_str());
		ImGui::Separator();

		if (ImGui::MenuItem("新規シーン", nullptr, false, canSave)) NewScene();
		ImGui::Separator();

		if (ImGui::MenuItem("シーンを保存", "Ctrl+S", false, canSave)) SaveCurrentScene();

		if (ImGui::MenuItem("名前を付けて保存...", nullptr, false, canSave))
		{
			// 今のファイル名（拡張子なし）を入れておく
			strncpy_s(m_SaveAsBuffer, AssetBrowser::GetStem(m_ScenePath).c_str(), _TRUNCATE);
			m_OpenSaveAsPopup = true;
		}

		// asset\scene にある .json を一覧にする
		if (ImGui::BeginMenu("シーンを開く"))
		{
			std::vector<std::string> scenes = EditorInternal::GetSceneFiles();
			for (const std::string& path : scenes)
			{
				std::string fileName = AssetBrowser::GetFileName(path);
				if (ImGui::MenuItem(fileName.c_str(), nullptr, path == m_ScenePath)) OpenScene(path);
			}
			if (scenes.empty()) ImGui::TextDisabled("asset\\scene に .json がありません");
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

	// Unity のように中央に並べる
	float h = ImGui::GetFrameHeight();
	float groupWidth = h * 1.4f * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
	ImGui::SetCursorPosX((ImGui::GetWindowWidth() - groupWidth) * 0.5f);

	// ▶：止まっていれば再生。Play 中は押しても何もしない
	if (EditorUI::IconButton("##Play", EditorUI::Icon::Play, m_PlayState == PlayState::Play, "再生 (Ctrl+P)", 1.4f))
	{
		if (m_PlayState != PlayState::Play) Play();
	}

	// ❚❚：Play 中なら一時停止、一時停止中なら再開
	if (EditorUI::IconButton("##Pause", EditorUI::Icon::Pause, m_PlayState == PlayState::Pause, "一時停止 (Ctrl+Shift+P)", 1.4f))
	{
		TogglePause();
	}

	// ■：シーンを読み込み直して最初の状態に戻す（読み込み後の1フレーム更新は Manager 側で行う）
	if (EditorUI::IconButton("##Stop", EditorUI::Icon::Stop, false, "停止・最初の状態に戻す (Ctrl+P)", 1.4f))
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
