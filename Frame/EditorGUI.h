#pragma once

#include <string>

class GameObject;
class Vector3;
struct ImDrawList;

enum class PlayState
{
	Edit,	// 止まっている。エディタカメラで見回せる
	Play,	// ゲームが動いている
	Pause,	// 一時停止中（Step で1フレームずつ進められる）
};

// ギズモの操作モード
enum class GizmoOperation
{
	Translate,	// 移動
	Rotate,		// 回転
	Scale,		// 拡縮
};

// ツールバー / Scene / Hierarchy / Inspector ウィンドウ
class EditorGUI
{
private:
	static unsigned int m_SelectedID;		// 選択中のオブジェクト（ポインタだと削除後に危険なので ID で持つ）

	static GameObject* m_DragChild;			// ドラッグ＆ドロップで親を変える予約
	static GameObject* m_DropParent;
	static bool        m_HasDrop;

	static PlayState   m_PlayState;
	static int         m_StepFrames;		// 止まっている間に進めるフレーム数

	static bool        m_SceneHovered;		// シーンビューの画像にマウスが乗っているか
	static ImDrawList* m_SceneDrawList;		// Gizmo をシーンビューに描くため
	static float       m_SceneMin[2];
	static float       m_SceneMax[2];

	static GizmoOperation m_GizmoOperation;
	static void        DrawGizmoToolbar();
	static bool        m_GizmoLocal;		// true:ローカル座標 false:ワールド座標
	static bool        m_GizmoActive;		// ギズモにマウスが乗っている／ドラッグ中

	static std::string m_ScenePath;			// 今のシーンファイル（Ctrl+S の保存先）
	static std::string m_PlaySnapshot;		// Play を押す直前のシーン（Stop で戻す）
	static bool        m_HasSnapshot;
	static char        m_SaveAsBuffer[260];
	static bool        m_OpenSaveAsPopup;
	static bool        m_DuplicateRequested;	// Hierarchy の右クリックから複製を頼まれた

	static void DrawToolbar();
	static void DrawFileMenu();
	static void SaveScene(const std::string& path);
	static void DrawTransformGizmo();
	static void PickObject();
	static void FocusObject(unsigned int id);
	static void HandleShortcuts();	// Ctrl+Z/Y 元に戻す/やり直し、Ctrl+D 複製、Delete 削除、F2 名前の変更、Ctrl+P 再生
	static void DrawEditMenu();
	static void TrackUndo(bool frameEnd);	// マウス操作の始まりと終わりで元に戻す用の状態を記録
	static void ApplyHistory(bool redo);	// 元に戻す／やり直しを実行し、選び直す名前を覚える

	// 再生・一時停止・停止（ボタンとショートカットで共通）
	static void Play();
	static void TogglePause();
	static void Stop();

	// 名前の変更
	static unsigned int m_RenamingID;
	static char         m_RenameBuffer[128];
	static bool         m_RenameFocus;
	static void BeginRename(unsigned int id);

	static std::string  m_RestoreSelectName;	// 元に戻したあと、同じ名前のオブジェクトを選び直す
	static Vector3 GetDropPosition();
	static void DrawSceneView();
	static void DrawHierarchy();
	static void DrawNode(GameObject* object);
	static void DrawInspector();

public:
	static void Draw();		// ImGui::NewFrame と ImGui::Render の間で呼ぶ

	// シーンファイルを開く（止めた状態で読み込む）
	static void OpenScene(const std::string& path);

	// Play/Pause/Stop
	static PlayState GetPlayState() { return m_PlayState; }
	static bool ConsumeGameUpdate();		// このフレームにゲームを更新するか（Step の消費も行う）
	static bool UseEditorCamera() { return m_PlayState != PlayState::Play; }
	static void RequestStep(int frames = 1) { m_StepFrames += frames; }

	// シーンビュー
	static bool        IsSceneViewHovered() { return m_SceneHovered; }
	static bool        IsGizmoActive() { return m_GizmoActive; }
	static ImDrawList* GetSceneDrawList() { return m_SceneDrawList; }
	static void        GetSceneRect(float* minX, float* minY, float* maxX, float* maxY)
	{
		*minX = m_SceneMin[0]; *minY = m_SceneMin[1]; *maxX = m_SceneMax[0]; *maxY = m_SceneMax[1];
	}
};
