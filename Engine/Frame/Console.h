#pragma once
#include "EngineAPI.h"
#include <string>
#include <vector>

enum class LogLevel { Info, Warning, Error };

// スクリプトから使うログ出力（Console ウィンドウと Visual Studio の出力ウィンドウの両方に出る）
// 例: Debug::Log("HP: %d", hp);
class ENGINE_API Debug
{
public:
	static void Log(const char* format, ...);
	static void LogWarning(const char* format, ...);
	static void LogError(const char* format, ...);
};

class ENGINE_API Console
{
private:
	struct Entry
	{
		LogLevel    Level;
		std::string Text;
		std::string Time;
	};

	static std::vector<Entry> m_Entries;
	static int  m_Counts[3];
	static bool m_Show[3];			// Info / Warning / Error の表示切り替え
	static bool m_Collapse;			// 同じ内容をまとめる
	static bool m_ClearOnPlay;
	static bool m_AutoScroll;
	static char m_Filter[128];
	static int  m_Selected;
	static bool m_ScrollToBottom;

	static const size_t MAX_ENTRIES = 2000;

public:
	static void Add(LogLevel level, const std::string& text);
	static void Clear();
	static void OnPlay() { if (m_ClearOnPlay) Clear(); }
	static void Draw(bool* open);
};
