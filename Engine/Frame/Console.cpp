#include "main.h"
#include "Console.h"
#include <cstdarg>
#include <map>

std::vector<Console::Entry> Console::m_Entries;
int  Console::m_Counts[3] = { 0, 0, 0 };
bool Console::m_Show[3] = { true, true, true };
bool Console::m_Collapse = false;
bool Console::m_ClearOnPlay = false;
bool Console::m_AutoScroll = true;
char Console::m_Filter[128] = "";
int  Console::m_Selected = -1;
bool Console::m_ScrollToBottom = false;

//=============================================================
// Debug::Log など
//=============================================================
static void AddFormatted(LogLevel level, const char* format, va_list args)
{
	char buffer[2048];
	vsnprintf(buffer, sizeof(buffer), format, args);
	Console::Add(level, buffer);
}

void Debug::Log(const char* format, ...)        { va_list a; va_start(a, format); AddFormatted(LogLevel::Info, format, a);    va_end(a); }
void Debug::LogWarning(const char* format, ...) { va_list a; va_start(a, format); AddFormatted(LogLevel::Warning, format, a); va_end(a); }
void Debug::LogError(const char* format, ...)   { va_list a; va_start(a, format); AddFormatted(LogLevel::Error, format, a);   va_end(a); }

//=============================================================
// 追加・消去
//=============================================================
void Console::Add(LogLevel level, const std::string& text)
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	char time[16];
	snprintf(time, sizeof(time), "%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);

	m_Entries.push_back({ level, text, time });
	m_Counts[(int)level]++;

	// 古いものから捨てる
	if (m_Entries.size() > MAX_ENTRIES)
	{
		m_Counts[(int)m_Entries.front().Level]--;
		m_Entries.erase(m_Entries.begin());
		if (m_Selected > 0) m_Selected--;
	}

	m_ScrollToBottom = m_AutoScroll;

	static const char* prefix[] = { "[Log] ", "[Warning] ", "[Error] " };
	// 出力ウィンドウは UTF-8 をそのまま出すと文字化けするのでワイド文字にする
	std::string line = prefix[(int)level] + text + "\n";
	int size = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, nullptr, 0);
	if (size > 0)
	{
		std::wstring wide(size, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, &wide[0], size);
		OutputDebugStringW(wide.c_str());
	}
}

void Console::Clear()
{
	m_Entries.clear();
	m_Counts[0] = m_Counts[1] = m_Counts[2] = 0;
	m_Selected = -1;
}

//=============================================================
// ウィンドウ
//=============================================================
static ImU32 LevelColor(LogLevel level)
{
	switch (level)
	{
	case LogLevel::Warning: return IM_COL32(240, 190, 60, 255);
	case LogLevel::Error:   return IM_COL32(235, 80, 80, 255);
	default:                return IM_COL32(170, 170, 180, 255);
	}
}

// 種類のアイコン（丸に記号）
static void DrawLevelIcon(ImDrawList* dl, ImVec2 center, float radius, LogLevel level)
{
	ImU32 color = LevelColor(level);
	if (level == LogLevel::Warning)
	{
		dl->AddTriangleFilled(ImVec2(center.x, center.y - radius), ImVec2(center.x + radius, center.y + radius * 0.8f),
			ImVec2(center.x - radius, center.y + radius * 0.8f), color);
		dl->AddLine(ImVec2(center.x, center.y - radius * 0.35f), ImVec2(center.x, center.y + radius * 0.2f), IM_COL32(40, 30, 0, 255), 1.5f);
		dl->AddCircleFilled(ImVec2(center.x, center.y + radius * 0.5f), 1.0f, IM_COL32(40, 30, 0, 255));
	}
	else
	{
		dl->AddCircleFilled(center, radius, color);
		ImU32 mark = IM_COL32(30, 30, 30, 255);
		if (level == LogLevel::Error)
		{
			dl->AddLine(ImVec2(center.x, center.y - radius * 0.5f), ImVec2(center.x, center.y + radius * 0.15f), mark, 1.5f);
			dl->AddCircleFilled(ImVec2(center.x, center.y + radius * 0.5f), 1.0f, mark);
		}
		else
		{
			dl->AddCircleFilled(ImVec2(center.x, center.y - radius * 0.45f), 1.0f, mark);
			dl->AddLine(ImVec2(center.x, center.y - radius * 0.1f), ImVec2(center.x, center.y + radius * 0.55f), mark, 1.5f);
		}
	}
}

void Console::Draw(bool* open)
{
	if (!ImGui::Begin("Console", open))
	{
		ImGui::End();
		return;
	}

	//---------------- ツールバー ----------------
	if (ImGui::Button("Clear")) Clear();
	ImGui::SameLine();
	ImGui::Checkbox("Collapse", &m_Collapse);
	ImGui::SameLine();
	ImGui::Checkbox("Clear on Play", &m_ClearOnPlay);
	ImGui::SameLine();
	ImGui::Checkbox("自動スクロール", &m_AutoScroll);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 190.0f);
	ImGui::InputTextWithHint("##filter", "検索", m_Filter, sizeof(m_Filter));

	// 右端：種類ごとの件数と表示切り替え
	for (int i = 0; i < 3; i++)
	{
		ImGui::SameLine();
		char label[32];
		snprintf(label, sizeof(label), "     %d##level%d", m_Counts[i] > 999 ? 999 : m_Counts[i], i);
		// ※ ボタンを押すと m_Show[i] が変わるので、Push したかどうかは先に覚えておく
		//    （押した瞬間だけ Push と Pop の数が合わなくなり、ImGui のチェックに引っかかっていた）
		bool dimmed = !m_Show[i];
		if (dimmed) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.45f);
		if (ImGui::Button(label, ImVec2(56.0f, 0.0f))) m_Show[i] = !m_Show[i];
		if (dimmed) ImGui::PopStyleVar();
		ImVec2 min = ImGui::GetItemRectMin();
		float h = ImGui::GetItemRectSize().y;
		DrawLevelIcon(ImGui::GetWindowDrawList(), ImVec2(min.x + 12.0f, min.y + h * 0.5f), h * 0.3f, (LogLevel)i);
	}
	ImGui::Separator();

	//---------------- 一覧に出すものを集める ----------------
	struct Row { int Index; int Count; };
	std::vector<Row> rows;
	std::map<std::string, size_t> collapsed;	// 同じ内容 → rows の位置

	for (int i = 0; i < (int)m_Entries.size(); i++)
	{
		const Entry& entry = m_Entries[i];
		if (!m_Show[(int)entry.Level]) continue;
		if (m_Filter[0] != '\0' && entry.Text.find(m_Filter) == std::string::npos) continue;

		if (m_Collapse)
		{
			std::string key = std::to_string((int)entry.Level) + entry.Text;
			auto it = collapsed.find(key);
			if (it != collapsed.end()) { rows[it->second].Count++; continue; }
			collapsed[key] = rows.size();
		}
		rows.push_back({ i, 1 });
	}

	//---------------- 一覧 ----------------
	float detailHeight = (m_Selected >= 0) ? 80.0f : 0.0f;
	ImGui::BeginChild("##ConsoleList", ImVec2(0, -detailHeight), ImGuiChildFlags_None);

	ImGuiListClipper clipper;
	clipper.Begin((int)rows.size());
	while (clipper.Step())
	{
		for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; r++)
		{
			const Entry& entry = m_Entries[rows[r].Index];
			ImGui::PushID(rows[r].Index);

			// 1行目だけ表示（改行以降は下の詳細に出す）
			std::string firstLine = entry.Text.substr(0, entry.Text.find('\n'));
			char label[512];
			snprintf(label, sizeof(label), "      [%s] %s", entry.Time.c_str(), firstLine.c_str());

			if (ImGui::Selectable(label, m_Selected == rows[r].Index)) m_Selected = rows[r].Index;

			// 右クリック：コピー
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("コピー")) ImGui::SetClipboardText(entry.Text.c_str());
				ImGui::EndPopup();
			}

			ImVec2 min = ImGui::GetItemRectMin();
			float h = ImGui::GetItemRectSize().y;
			DrawLevelIcon(ImGui::GetWindowDrawList(), ImVec2(min.x + 10.0f, min.y + h * 0.5f), h * 0.32f, entry.Level);

			if (rows[r].Count > 1)
			{
				char count[16];
				snprintf(count, sizeof(count), "%d", rows[r].Count);
				ImVec2 max = ImGui::GetItemRectMax();
				ImVec2 size = ImGui::CalcTextSize(count);
				ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(max.x - size.x - 14.0f, min.y + 1.0f), ImVec2(max.x - 4.0f, max.y - 1.0f), IM_COL32(90, 90, 100, 255), 8.0f);
				ImGui::GetWindowDrawList()->AddText(ImVec2(max.x - size.x - 9.0f, min.y + (h - size.y) * 0.5f), IM_COL32(255, 255, 255, 255), count);
			}
			ImGui::PopID();
		}
	}

	if (m_ScrollToBottom && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 40.0f) ImGui::SetScrollHereY(1.0f);
	m_ScrollToBottom = false;
	ImGui::EndChild();

	//---------------- 選んだログの全文 ----------------
	if (m_Selected >= 0 && m_Selected < (int)m_Entries.size())
	{
		ImGui::Separator();
		ImGui::BeginChild("##ConsoleDetail");
		ImGui::TextWrapped("%s", m_Entries[m_Selected].Text.c_str());
		ImGui::EndChild();
	}

	ImGui::End();
}
