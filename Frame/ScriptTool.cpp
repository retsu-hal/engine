#include "main.h"
#include "ScriptTool.h"
#include "Registry.h"
#include "JsonUtil.h"	// Utf8ToWide / WideToUtf8
#include "Console.h"
#include <fstream>
#include <sstream>
#include <cctype>

//=============================================================
// ひな形
//=============================================================
static std::string HeaderTemplate(const std::string& name)
{
	return
		"#pragma once\n"
		"#include \"Component.h\"\n"
		"\n"
		"class " + name + " : public Component\n"
		"{\n"
		"private:\n"
		"\tfloat m_Speed = 1.0f;\t// 例：Inspector で変えられる値\n"
		"\n"
		"public:\n"
		"\tusing Component::Component;\n"
		"\n"
		"\tvoid Start() override;\t\t\t\t\t\t// 最初の Update の直前に1回\n"
		"\tvoid Update() override;\t\t\t\t\t\t// 毎フレーム\n"
		"\tvoid OnCollision(GameObject* other) override;\t// 当たっている間\n"
		"\n"
		"\tvoid OnInspectorGUI() override;\t\t\t\t// Inspector に出す項目\n"
		"\tvoid Serialize(nlohmann::json& data) const override;\t// シーンに保存する値\n"
		"\tvoid Deserialize(const nlohmann::json& data) override;\n"
		"};\n";
}

static std::string SourceTemplate(const std::string& name)
{
	return
		"#include \"main.h\"\n"
		"#include \"Renderer.h\"\n"
		"#include \"Manager.h\"\n"
		"#include \"GameObject.h\"\n"
		"#include \"Console.h\"\n"
		"#include \"JsonUtil.h\"\n"
		"#include \"Registry.h\"\n"
		"#include \"" + name + ".h\"\n"
		"\n"
		"void " + name + "::Start()\n"
		"{\n"
		"\tDebug::Log(\"" + name + " が始まりました: %s\", m_GameObject->GetName().c_str());\n"
		"}\n"
		"\n"
		"void " + name + "::Update()\n"
		"{\n"
		"\tfloat dt = Manager::GetDeltaTime();\n"
		"\n"
		"\t// 例：Y 軸まわりに回す\n"
		"\tVector3 rotation = m_GameObject->GetRotation();\n"
		"\trotation.y += m_Speed * dt;\n"
		"\tm_GameObject->SetRotation(rotation);\n"
		"}\n"
		"\n"
		"void " + name + "::OnCollision(GameObject* other)\n"
		"{\n"
		"}\n"
		"\n"
		"void " + name + "::OnInspectorGUI()\n"
		"{\n"
		"\tImGui::DragFloat(\"Speed\", &m_Speed, 0.1f);\n"
		"}\n"
		"\n"
		"void " + name + "::Serialize(nlohmann::json& data) const\n"
		"{\n"
		"\tdata[\"speed\"] = m_Speed;\n"
		"}\n"
		"\n"
		"void " + name + "::Deserialize(const nlohmann::json& data)\n"
		"{\n"
		"\tJsonRead(data, \"speed\", m_Speed);\n"
		"}\n"
		"\n"
		"// これで名前から作れるようになり、Add Component に出る\n"
		"REGISTER_COMPONENT(" + name + ")\n";
}

// BOM 付き UTF-8・CRLF で書く（Visual Studio で日本語が化けないように）
static bool WriteSourceFile(const std::wstring& path, const std::string& text)
{
	std::string crlf;
	for (char c : text)
	{
		if (c == '\n') crlf += "\r\n";
		else crlf += c;
	}

	std::ofstream file(path, std::ios::binary);
	if (!file) return false;
	file << "\xEF\xBB\xBF" << crlf;
	return true;
}

//=============================================================
// vcxproj に追加
//=============================================================
static std::string FindProjectFile(const char* extension)
{
	WIN32_FIND_DATAA find;
	HANDLE handle = FindFirstFileA((std::string("*") + extension).c_str(), &find);
	if (handle == INVALID_HANDLE_VALUE) return std::string();
	std::string name = find.cFileName;
	FindClose(handle);
	return name;
}

static std::string ReadAll(const std::string& path)
{
	std::ifstream file(path, std::ios::binary);
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

// tag（ClCompile / ClInclude）の最後の項目の後ろに1行足す
static bool InsertItem(std::string& text, const std::string& tag, const std::string& item, const std::string& newline)
{
	std::string key = "<" + tag + " Include=\"";
	size_t last = text.rfind(key);
	if (last == std::string::npos) return false;

	// その項目の終わり（1行の <X ... /> か、複数行の </X>）
	size_t selfClose = text.find("/>", last);
	size_t lineEnd = text.find('\n', last);
	size_t end;
	if (selfClose != std::string::npos && selfClose < lineEnd)
	{
		end = lineEnd + 1;
	}
	else
	{
		size_t close = text.find("</" + tag + ">", last);
		if (close == std::string::npos) return false;
		end = text.find('\n', close) + 1;
	}

	text.insert(end, item + newline);
	return true;
}

static bool AddToProject(const std::string& headerPath, const std::string& sourcePath)
{
	std::string vcxproj = FindProjectFile(".vcxproj");
	if (vcxproj.empty()) return false;

	std::string text = ReadAll(vcxproj);
	if (text.find("Include=\"" + sourcePath + "\"") != std::string::npos) return true;	// もう入っている
	std::string nl = (text.find("\r\n") != std::string::npos) ? "\r\n" : "\n";

	bool ok = InsertItem(text, "ClCompile", "    <ClCompile Include=\"" + sourcePath + "\" />", nl)
		&& InsertItem(text, "ClInclude", "    <ClInclude Include=\"" + headerPath + "\" />", nl);
	if (!ok) return false;
	std::ofstream(vcxproj, std::ios::binary) << text;

	// ソリューションエクスプローラーの Script フィルタに入れる
	std::string filters = vcxproj + ".filters";
	std::string filterText = ReadAll(filters);
	if (!filterText.empty())
	{
		InsertItem(filterText, "ClCompile", "    <ClCompile Include=\"" + sourcePath + "\">" + nl + "      <Filter>Script</Filter>" + nl + "    </ClCompile>", nl);
		InsertItem(filterText, "ClInclude", "    <ClInclude Include=\"" + headerPath + "\">" + nl + "      <Filter>Script</Filter>" + nl + "    </ClInclude>", nl);
		std::ofstream(filters, std::ios::binary) << filterText;
	}
	return true;
}

//=============================================================
// 作成
//=============================================================
bool ScriptTool::IsValidClassName(const std::string& name)
{
	if (name.empty()) return false;
	if (!(std::isalpha((unsigned char)name[0]) || name[0] == '_')) return false;
	for (char c : name)
		if (!(std::isalnum((unsigned char)c) || c == '_')) return false;
	return true;
}

bool ScriptTool::CreateScript(const std::string& folder, const std::string& className, std::string& outError)
{
	if (!IsValidClassName(className))
	{
		outError = "クラス名は英字で始め、英数字と _ だけにしてください";
		return false;
	}
	if (ComponentRegistry::GetAll().count(className) > 0)
	{
		outError = "同じ名前のコンポーネントがすでにあります";
		return false;
	}

	std::string headerPath = folder + "\\" + className + ".h";
	std::string sourcePath = folder + "\\" + className + ".cpp";
	if (GetFileAttributesA(headerPath.c_str()) != INVALID_FILE_ATTRIBUTES || GetFileAttributesA(sourcePath.c_str()) != INVALID_FILE_ATTRIBUTES)
	{
		outError = "同じ名前のファイルがすでにあります";
		return false;
	}

	CreateDirectoryA(folder.c_str(), nullptr);
	if (!WriteSourceFile(Utf8ToWide(headerPath), HeaderTemplate(className)) ||
		!WriteSourceFile(Utf8ToWide(sourcePath), SourceTemplate(className)))
	{
		outError = "ファイルを書き込めませんでした";
		return false;
	}

	if (AddToProject(headerPath, sourcePath))
	{
		Debug::Log("スクリプト %s を作成しました。Visual Studio でプロジェクトを再読み込みしてビルドすると Add Component に表示されます", className.c_str());
	}
	else
	{
		Debug::LogWarning("スクリプト %s を作成しましたが、プロジェクトに追加できませんでした。Visual Studio で「既存の項目を追加」してください", className.c_str());
	}
	return true;
}

//=============================================================
// Visual Studio で開く
//=============================================================
// vswhere（Visual Studio に付いてくるツール）で devenv.exe の場所を調べる
static std::wstring FindDevenv()
{
	static bool searched = false;
	static std::wstring devenv;
	if (searched) return devenv;
	searched = true;

	wchar_t programFiles[MAX_PATH];
	if (GetEnvironmentVariableW(L"ProgramFiles(x86)", programFiles, MAX_PATH) == 0) return devenv;
	std::wstring vswhere = std::wstring(programFiles) + L"\\Microsoft Visual Studio\\Installer\\vswhere.exe";
	if (GetFileAttributesW(vswhere.c_str()) == INVALID_FILE_ATTRIBUTES) return devenv;

	SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
	HANDLE readPipe, writePipe;
	if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return devenv;
	SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOW si{};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = writePipe;
	si.hStdError = writePipe;
	PROCESS_INFORMATION pi{};

	std::wstring command = L"\"" + vswhere + L"\" -latest -prerelease -property productPath";
	if (CreateProcessW(nullptr, &command[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
	{
		CloseHandle(writePipe);
		writePipe = nullptr;

		std::string output;
		char buffer[512];
		DWORD read;
		while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) output.append(buffer, read);
		WaitForSingleObject(pi.hProcess, 3000);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		while (!output.empty() && (output.back() == '\r' || output.back() == '\n' || output.back() == ' ')) output.pop_back();
		size_t firstLine = output.find('\n');
		if (firstLine != std::string::npos) output = output.substr(0, firstLine);
		while (!output.empty() && output.back() == '\r') output.pop_back();
		if (!output.empty() && GetFileAttributesA(output.c_str()) != INVALID_FILE_ATTRIBUTES) devenv = Utf8ToWide(output);
	}
	if (writePipe) CloseHandle(writePipe);
	CloseHandle(readPipe);
	return devenv;
}

void ScriptTool::OpenInEditor(const std::string& path)
{
	wchar_t fullPath[MAX_PATH];
	GetFullPathNameW(Utf8ToWide(path).c_str(), MAX_PATH, fullPath, nullptr);

	// /Edit を付けると、開いている Visual Studio のウィンドウでファイルを開く
	std::wstring devenv = FindDevenv();
	if (!devenv.empty())
	{
		std::wstring parameter = L"/Edit \"" + std::wstring(fullPath) + L"\"";
		if ((INT_PTR)ShellExecuteW(nullptr, L"open", devenv.c_str(), parameter.c_str(), nullptr, SW_SHOWNORMAL) > 32) return;
	}

	ShellExecuteW(nullptr, L"open", fullPath, nullptr, nullptr, SW_SHOWNORMAL);
}

std::string ScriptTool::FindSourceFile(const std::string& typeName)
{
	const char* folders[] = { "Script", "Component", "Frame" };
	for (const char* folder : folders)
	{
		std::string path = std::string(folder) + "\\" + typeName + ".cpp";
		if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) return path;
	}
	return std::string();
}
