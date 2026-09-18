#include "main.h"
#include "EnginePaths.h"

static std::string FullPath(const std::string& path)
{
	char buffer[MAX_PATH];
	DWORD length = GetFullPathNameA(path.c_str(), MAX_PATH, buffer, nullptr);
	if (length == 0 || length >= MAX_PATH) return path;
	std::string result = buffer;
	while (result.size() > 3 && (result.back() == '\\' || result.back() == '/')) result.pop_back();
	return result;
}

bool EnginePaths::Exists(const std::string& path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::string EnginePaths::GetExeDir()
{
	char buffer[MAX_PATH];
	GetModuleFileNameA(nullptr, buffer, MAX_PATH);
	std::string path = buffer;
	size_t slash = path.find_last_of("\\/");
	return (slash == std::string::npos) ? std::string(".") : path.substr(0, slash);
}

// exe のフォルダから親へさかのぼって、Engine フォルダ（EngineAPI.h がある）を探す
// ※ フォルダ名や .sln の名前を変えても見つかるように、中身で判断する
static std::string FindUpward(const std::string& marker)
{
	std::string dir = EnginePaths::GetExeDir();
	for (int i = 0; i < 6; i++)
	{
		if (EnginePaths::Exists(dir + "\\" + marker)) return dir;
		std::string parent = FullPath(dir + "\\..");
		if (parent == dir) break;
		dir = parent;
	}
	return std::string();
}

std::string EnginePaths::GetSolutionDir()
{
	// Engine\EngineAPI.h があるフォルダ（＝ .sln のあるフォルダ）
	return FindUpward("Engine\\EngineAPI.h");
}

std::string EnginePaths::GetEngineDir()
{
	std::string solution = GetSolutionDir();
	if (!solution.empty()) return solution + "\\Engine";
	return GetExeDir();	// 配布したゲーム：exe の横に shader がある
}

std::string EnginePaths::GetProjectDir()
{
	return FullPath(".");
}

std::string EnginePaths::ResolveEngineFile(const std::string& relativePath)
{
	const std::string candidates[] = {
		relativePath,								// 作業フォルダ（配布したゲーム）
		GetExeDir() + "\\" + relativePath,			// exe の横
		GetEngineDir() + "\\" + relativePath,		// 開発中：Engine フォルダ
	};
	for (const std::string& path : candidates)
	{
		if (Exists(path)) return path;
	}

	// 見つからない：どこを探したか出しておく
	std::string message = "[EnginePaths] 見つかりません: " + relativePath + "\n";
	for (const std::string& path : candidates) message += "  探した場所: " + FullPath(path) + "\n";
	int size = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, nullptr, 0);
	std::wstring wide(size, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, &wide[0], size);
	OutputDebugStringW(wide.c_str());

	return relativePath;
}
