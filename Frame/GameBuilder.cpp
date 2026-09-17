#include "main.h"
#include "GameBuilder.h"
#include "ProjectSettings.h"
#include "Console.h"
#include <algorithm>
#include "JsonUtil.h"	// Utf8ToWide / WideToUtf8

static GameBuilder::State s_State = GameBuilder::State::Idle;
static HANDLE      s_Process = nullptr;
static HANDLE      s_ReadPipe = nullptr;
static std::string s_LineBuffer;
static bool        s_RunAfterBuild = false;
static DWORD       s_StartTick = 0;
static std::string s_OutputFolder = "Build";

//=============================================================
// 準備
//=============================================================
static std::string FindFile(const char* pattern)
{
	WIN32_FIND_DATAA find;
	HANDLE handle = FindFirstFileA(pattern, &find);
	if (handle == INVALID_HANDLE_VALUE) return std::string();
	std::string name = find.cFileName;
	FindClose(handle);
	return name;
}

// コマンドを実行して標準出力をまとめて受け取る（短い処理用）
static std::string RunAndRead(const std::wstring& commandLine, DWORD* exitCode = nullptr)
{
	SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
	HANDLE readPipe, writePipe;
	if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return std::string();
	SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOW si{};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = writePipe;
	si.hStdError = writePipe;
	PROCESS_INFORMATION pi{};

	std::wstring command = commandLine;
	std::string output;
	if (CreateProcessW(nullptr, &command[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
	{
		CloseHandle(writePipe);
		writePipe = nullptr;
		char buffer[1024];
		DWORD read;
		while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) output.append(buffer, read);
		WaitForSingleObject(pi.hProcess, INFINITE);
		if (exitCode) GetExitCodeProcess(pi.hProcess, exitCode);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	if (writePipe) CloseHandle(writePipe);
	CloseHandle(readPipe);
	return output;
}

static std::wstring FindMSBuild()
{
	wchar_t programFiles[MAX_PATH];
	if (GetEnvironmentVariableW(L"ProgramFiles(x86)", programFiles, MAX_PATH) == 0) return std::wstring();
	std::wstring vswhere = std::wstring(programFiles) + L"\\Microsoft Visual Studio\\Installer\\vswhere.exe";
	if (GetFileAttributesW(vswhere.c_str()) == INVALID_FILE_ATTRIBUTES) return std::wstring();

	std::string output = RunAndRead(L"\"" + vswhere + L"\" -latest -prerelease -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\MSBuild.exe");
	size_t end = output.find_first_of("\r\n");
	if (end != std::string::npos) output = output.substr(0, end);
	if (output.empty() || GetFileAttributesA(output.c_str()) == INVALID_FILE_ATTRIBUTES) return std::wstring();
	return Utf8ToWide(output);
}

//=============================================================
// 開始
//=============================================================
void GameBuilder::Start(bool runAfterBuild)
{
	if (s_State == State::Building) return;

	std::string project = FindFile("*.vcxproj");
	std::wstring msbuild = FindMSBuild();
	if (project.empty() || msbuild.empty())
	{
		Debug::LogError("ビルドできません: %s", project.empty() ? "vcxproj が見つかりません" : "MSBuild が見つかりません（Visual Studio が必要です）");
		s_State = State::Failed;
		return;
	}

	ProjectSettings::Save();	// ゲームが読む設定を先に書いておく

	wchar_t currentDir[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, currentDir);
	std::wstring projectDir = std::wstring(currentDir) + L"\\";

	// /m:並列ビルド  /v:minimal:出力を少なく
	std::wstring command = L"\"" + msbuild + L"\" \"" + Utf8ToWide(project) +
		L"\" /p:Configuration=Game /p:Platform=x64 /p:SolutionDir=\"" + projectDir + L"\\\" /m /nologo /v:minimal";

	SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
	HANDLE writePipe;
	if (!CreatePipe(&s_ReadPipe, &writePipe, &sa, 0)) return;
	SetHandleInformation(s_ReadPipe, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOW si{};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = writePipe;
	si.hStdError = writePipe;
	PROCESS_INFORMATION pi{};

	if (!CreateProcessW(nullptr, &command[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
	{
		CloseHandle(writePipe);
		CloseHandle(s_ReadPipe);
		s_ReadPipe = nullptr;
		Debug::LogError("MSBuild を起動できませんでした");
		s_State = State::Failed;
		return;
	}
	CloseHandle(writePipe);
	CloseHandle(pi.hThread);

	s_Process = pi.hProcess;
	s_LineBuffer.clear();
	s_RunAfterBuild = runAfterBuild;
	s_StartTick = GetTickCount();
	s_State = State::Building;
	Debug::Log("ゲームのビルドを開始しました（Game 構成）");
}

//=============================================================
// 出力をまとめる
//=============================================================
static bool CopyToOutput()
{
	std::string exe = "x64\\Game\\" + FindFile("*.vcxproj").substr(0, FindFile("*.vcxproj").size() - 8) + ".exe";
	if (GetFileAttributesA(exe.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		Debug::LogError("exe が見つかりません: %s", exe.c_str());
		return false;
	}

	CreateDirectoryA(s_OutputFolder.c_str(), nullptr);
	CreateDirectoryA((s_OutputFolder + "\\shader").c_str(), nullptr);

	std::string gameExe = s_OutputFolder + "\\" + ProjectSettings::Title + ".exe";
	for (char& c : gameExe) if (strchr(":*?\"<>|", c)) c = '_';
	if (!CopyFileW(Utf8ToWide(exe).c_str(), Utf8ToWide(gameExe).c_str(), FALSE))
	{
		Debug::LogError("exe をコピーできませんでした（ゲームが起動したままになっていませんか？）");
		return false;
	}

	// DLL とシェーダー
	WIN32_FIND_DATAA find;
	HANDLE handle = FindFirstFileA("*.dll", &find);
	if (handle != INVALID_HANDLE_VALUE)
	{
		do { CopyFileA(find.cFileName, (s_OutputFolder + "\\" + find.cFileName).c_str(), FALSE); } while (FindNextFileA(handle, &find));
		FindClose(handle);
	}
	handle = FindFirstFileA("shader\\*.cso", &find);
	if (handle != INVALID_HANDLE_VALUE)
	{
		do { CopyFileA((std::string("shader\\") + find.cFileName).c_str(), (s_OutputFolder + "\\shader\\" + find.cFileName).c_str(), FALSE); } while (FindNextFileA(handle, &find));
		FindClose(handle);
	}

	// asset フォルダは robocopy で丸ごと（変わったファイルだけコピーされる）
	DWORD exitCode = 0;
	RunAndRead(L"robocopy asset \"" + Utf8ToWide(s_OutputFolder) + L"\\asset\" /E /NFL /NDL /NJH /NJS /NP", &exitCode);
	if (exitCode >= 8)	// robocopy は 8 以上が失敗
	{
		Debug::LogError("asset のコピーに失敗しました（robocopy: %lu）", exitCode);
		return false;
	}

	Debug::Log("ゲームを %s にまとめました", s_OutputFolder.c_str());
	return true;
}

//=============================================================
// 毎フレーム
//=============================================================
void GameBuilder::Update()
{
	if (s_State != State::Building) return;

	// たまっている出力だけ読む（読むものがないときに止まらないように PeekNamedPipe で確かめる）
	DWORD available = 0;
	while (PeekNamedPipe(s_ReadPipe, nullptr, 0, nullptr, &available, nullptr) && available > 0)
	{
		char buffer[1024];
		DWORD read = 0;
		if (!ReadFile(s_ReadPipe, buffer, (DWORD)(std::min)((DWORD)sizeof(buffer), available), &read, nullptr) || read == 0) break;
		s_LineBuffer.append(buffer, read);

		size_t newline;
		while ((newline = s_LineBuffer.find('\n')) != std::string::npos)
		{
			std::string line = s_LineBuffer.substr(0, newline);
			s_LineBuffer.erase(0, newline + 1);
			while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
			if (line.empty()) continue;

			// MSBuild は日本語 Windows だと Shift-JIS で出すので UTF-8 に直す
			int wlen = MultiByteToWideChar(CP_ACP, 0, line.c_str(), (int)line.size(), nullptr, 0);
			std::wstring wide(wlen, L'\0');
			MultiByteToWideChar(CP_ACP, 0, line.c_str(), (int)line.size(), &wide[0], wlen);
			std::string utf8 = WideToUtf8(wide);

			if (utf8.find("error") != std::string::npos)        Console::Add(LogLevel::Error, "[Build] " + utf8);
			else if (utf8.find("warning") != std::string::npos) Console::Add(LogLevel::Warning, "[Build] " + utf8);
			else                                                Console::Add(LogLevel::Info, "[Build] " + utf8);
		}
	}

	if (WaitForSingleObject(s_Process, 0) != WAIT_OBJECT_0) return;	// まだ終わっていない

	DWORD exitCode = 1;
	GetExitCodeProcess(s_Process, &exitCode);
	CloseHandle(s_Process);
	CloseHandle(s_ReadPipe);
	s_Process = nullptr;
	s_ReadPipe = nullptr;

	if (exitCode != 0)
	{
		Debug::LogError("ビルドに失敗しました（Console のエラーを確認してください）");
		s_State = State::Failed;
		return;
	}

	if (!CopyToOutput())
	{
		s_State = State::Failed;
		return;
	}

	Debug::Log("ビルドが完了しました（%.1f 秒）", GetElapsedSeconds());
	s_State = State::Succeeded;

	if (s_RunAfterBuild)
	{
		std::wstring folder = Utf8ToWide(s_OutputFolder);
		std::wstring exe = folder + L"\\" + Utf8ToWide(ProjectSettings::Title) + L".exe";
		for (wchar_t& c : exe) if (wcschr(L":*?\"<>|", c) && &c != &exe[1]) c = L'_';
		ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, folder.c_str(), SW_SHOWNORMAL);	// 作業フォルダを Build にする
	}
}

GameBuilder::State GameBuilder::GetState() { return s_State; }
float GameBuilder::GetElapsedSeconds() { return (GetTickCount() - s_StartTick) / 1000.0f; }
const std::string& GameBuilder::GetOutputFolder() { return s_OutputFolder; }
void GameBuilder::OpenOutputFolder() { ShellExecuteW(nullptr, L"open", Utf8ToWide(s_OutputFolder).c_str(), nullptr, nullptr, SW_SHOWNORMAL); }
