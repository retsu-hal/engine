#pragma once

class GameObject;
struct ImDrawList;

enum class PlayState
{
	Edit,	// 止まっている。エディタカメラで見回せる
	Play,	// ゲームが動いている
	Pause,	// 一時停止中（Step で1フレームずつ進められる）
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

	static void DrawToolbar();
	static void DrawSceneView();
	static void DrawHierarchy();
	static void DrawNode(GameObject* object);
	static void DrawInspector();

public:
	static void Draw();		// ImGui::NewFrame と ImGui::Render の間で呼ぶ

	// Play/Pause/Stop
	static PlayState GetPlayState() { return m_PlayState; }
	static bool ConsumeGameUpdate();		// このフレームにゲームを更新するか（Step の消費も行う）
	static bool UseEditorCamera() { return m_PlayState != PlayState::Play; }
	static void RequestStep(int frames = 1) { m_StepFrames += frames; }

	// シーンビュー
	static bool        IsSceneViewHovered() { return m_SceneHovered; }
	static ImDrawList* GetSceneDrawList() { return m_SceneDrawList; }
	static void        GetSceneRect(float* minX, float* minY, float* maxX, float* maxY)
	{
		*minX = m_SceneMin[0]; *minY = m_SceneMin[1]; *maxX = m_SceneMax[0]; *maxY = m_SceneMax[1];
	}
};
