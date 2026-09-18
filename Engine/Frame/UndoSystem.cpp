#include "main.h"
#include "Manager.h"
#include "UndoSystem.h"
#include "SceneSerializer.h"

std::vector<std::string> UndoSystem::m_UndoStack;
std::vector<std::string> UndoSystem::m_RedoStack;
std::string              UndoSystem::m_Pending;
bool                     UndoSystem::m_HasPending = false;

// 今のシーンを文字列にする（保存できない物がある＝戻しても元どおりにならないので false）
static bool Capture(std::string& out)
{
	if (Manager::IsSceneChanging()) return false;
	return SceneSerializer::SaveToText(out) == 0;
}

void UndoSystem::Begin()
{
	if (m_HasPending) return;
	m_HasPending = Capture(m_Pending);
}

void UndoSystem::End()
{
	if (!m_HasPending) return;
	m_HasPending = false;

	std::string now;
	if (!Capture(now) || now == m_Pending) return;	// 何も変わっていない

	m_UndoStack.push_back(m_Pending);
	if (m_UndoStack.size() > MAX_HISTORY) m_UndoStack.erase(m_UndoStack.begin());
	m_RedoStack.clear();
}

void UndoSystem::Cancel()
{
	m_HasPending = false;
	m_Pending.clear();
}

bool UndoSystem::Undo()
{
	End();
	std::string now;
	if (m_UndoStack.empty() || !Capture(now)) return false;

	m_RedoStack.push_back(now);
	Manager::LoadSceneText(m_UndoStack.back());
	m_UndoStack.pop_back();
	return true;
}

bool UndoSystem::Redo()
{
	End();
	std::string now;
	if (m_RedoStack.empty() || !Capture(now)) return false;

	m_UndoStack.push_back(now);
	Manager::LoadSceneText(m_RedoStack.back());
	m_RedoStack.pop_back();
	return true;
}

void UndoSystem::Clear()
{
	m_UndoStack.clear();
	m_RedoStack.clear();
	Cancel();
}
