#pragma once

class GameObject;

// Hierarchy / Inspector ウィンドウ
class EditorGUI
{
private:
	static unsigned int m_SelectedID;		// 選択中のオブジェクト（ポインタだと削除後に危険なので ID で持つ）

	static GameObject* m_DragChild;			// ドラッグ＆ドロップで親を変える予約
	static GameObject* m_DropParent;
	static bool        m_HasDrop;

	static void DrawHierarchy();
	static void DrawNode(GameObject* object);
	static void DrawInspector();

public:
	static void Draw();		
};
