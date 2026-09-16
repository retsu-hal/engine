#include "main.h"
#include "Profiler.h"
#include <algorithm>

std::vector<Profiler::Entry> Profiler::m_Entries;
double                       Profiler::m_SceneMs = 0.0;

void Profiler::Add(const std::string& name, double ms)
{
	for (auto& e : m_Entries)
	{
		if (e.Name == name) { e.TotalMs += ms; e.Count++; return; }
	}
	Entry e;
	e.Name = name;
	e.TotalMs = ms;
	e.Count = 1;
	m_Entries.push_back(e);
}

void Profiler::SetSceneTime(double ms)
{
	m_SceneMs = ms;
}

void Profiler::Clear()
{
	m_Entries.clear();
	m_SceneMs = 0.0;
}

void Profiler::Dump()
{
	std::vector<Entry> sorted = m_Entries;
	std::sort(sorted.begin(), sorted.end(),
		[](const Entry& a, const Entry& b) { return a.TotalMs > b.TotalMs; });

	char buf[256];
	double total = 0.0;

	OutputDebugStringA("---------------- Load Time ----------------\n");
	for (auto& e : sorted)
	{
		total += e.TotalMs;
		sprintf(buf, "%-24s %8.3f ms  x%-3d  avg %7.3f ms\n",
			e.Name.c_str(), e.TotalMs, e.Count, e.TotalMs / e.Count);
		OutputDebugStringA(buf);
	}
	// 計測できた分の合計(AddGameObject の積み上げ)
	sprintf(buf, "%-24s %8.3f ms\n", "TOTAL(measured)", total);
	OutputDebugStringA(buf);
	// Scene::Init 全体の実経過時間。上の合計との差が未計測部分
	sprintf(buf, "%-24s %8.3f ms  (unmeasured %7.3f ms)\n",
		"SCENE Init(real)", m_SceneMs, m_SceneMs - total);
	OutputDebugStringA(buf);
	OutputDebugStringA("-------------------------------------------\n");
}
