#pragma once
#include <string>
#include <vector>

//=============================================================
// 元に戻す / やり直し
// 操作を始める前のシーンを JSON で覚えておき、戻すときはそれを読み込み直す
// （コンポーネントごとに「戻し方」を書かなくても、保存できる物は全部戻せる）
//=============================================================
class UndoSystem
{
private:
	static std::vector<std::string> m_UndoStack;
	static std::vector<std::string> m_RedoStack;
	static std::string              m_Pending;		// 操作を始める前の状態
	static bool                     m_HasPending;

	static const size_t MAX_HISTORY = 50;

public:
	static void Begin();	// 操作を始める前に呼ぶ（すでに始まっていれば何もしない）
	static void End();		// 操作が終わったら呼ぶ（変わっていれば履歴に積む）
	static void Cancel();	// 覚えた状態を捨てる（Play を押したときなど）
	static bool IsPending() { return m_HasPending; }

	static bool Undo();
	static bool Redo();
	static bool CanUndo() { return !m_UndoStack.empty(); }
	static bool CanRedo() { return !m_RedoStack.empty(); }
	static void Clear();
};
