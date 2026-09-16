#pragma once
#include <string>
#include <vector>
#include <chrono>

class Profiler
{
public:
	struct Entry
	{
		std::string Name;
		double      TotalMs = 0.0;
		int         Count = 0;
	};

	static void Add(const std::string& name, double ms);
	static void SetSceneTime(double ms);	// シーン全体の実経過時間を記録
	static void Clear();
	static void Dump();   // 出力ウィンドウへ書き出し

private:
	static std::vector<Entry> m_Entries;
	static double             m_SceneMs;
};

// スコープを抜けた瞬間に経過時間を記録する
class ScopedTimer
{
public:
	ScopedTimer(const std::string& name)
		: m_Name(name), m_Start(std::chrono::high_resolution_clock::now()) {}

	~ScopedTimer()
	{
		auto end = std::chrono::high_resolution_clock::now();
		Profiler::Add(m_Name,
			std::chrono::duration<double, std::milli>(end - m_Start).count());
	}

private:
	std::string m_Name;
	std::chrono::high_resolution_clock::time_point m_Start;
};

// シーン全体のロード時間(実経過時間)を計測する
class ScopedSceneTimer
{
public:
	ScopedSceneTimer()
		: m_Start(std::chrono::high_resolution_clock::now()) {}

	~ScopedSceneTimer()
	{
		auto end = std::chrono::high_resolution_clock::now();
		Profiler::SetSceneTime(
			std::chrono::duration<double, std::milli>(end - m_Start).count());
	}

private:
	std::chrono::high_resolution_clock::time_point m_Start;
};

// typeid(T).name() は "class Box" を返すので前置きを削る
inline std::string TypeName(const char* raw)
{
	std::string s = raw;
	if (s.compare(0, 6, "class ") == 0) s.erase(0, 6);
	else if (s.compare(0, 7, "struct ") == 0) s.erase(0, 7);
	return s;
}
