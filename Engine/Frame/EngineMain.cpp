#include "main.h"
#include "EngineMain.h"
#include "EnginePaths.h"
#include "Manager.h"
#include "Renderer.h"
#include "Console.h"
#include "Registry.h"
#include "JsonUtil.h"	// Utf8ToWide / WideToUtf8

static const wchar_t* CLASS_NAME = L"GM31EngineWindow";

static HWND    s_Window = nullptr;
static HMODULE s_GameScripts = nullptr;

HWND GetWindow()
{
	return s_Window;
}

bool EngineMain::IsGameScriptsLoaded()
{
	return s_GameScripts != nullptr;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

//=============================================================
// 起動オプション
//   -project "C:\...\MyGame"  … そのフォルダをプロジェクトとして開く（作業フォルダにする）
//=============================================================
static void ApplyCommandLine()
{
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv == nullptr) return;

	for (int i = 1; i + 1 < argc; i++)
	{
		if (_wcsicmp(argv[i], L"-project") == 0)
		{
			if (!SetCurrentDirectoryW(argv[i + 1]))
			{
				MessageBoxW(nullptr, argv[i + 1], L"プロジェクトのフォルダが開けません", MB_OK | MB_ICONWARNING);
			}
			i++;
		}
	}
	LocalFree(argv);
}

//=============================================================
// ゲームのスクリプト（GameScripts.dll）を読み込む
// DLL の中の REGISTER_COMPONENT などが、読み込んだ瞬間にエンジンの登録表へ登録される
//=============================================================
static void LoadGameScripts()
{
	std::string path = EnginePaths::GetExeDir() + "\\GameScripts.dll";
	std::string loadPath = path;

#ifdef ENGINE_EDITOR
	// エディタでは別名のコピーを読み込む
	// （本物を読み込むとファイルがロックされ、エディタを開いたまま Visual Studio でビルドできなくなる）
	std::string copyPath = EnginePaths::GetExeDir() + "\\GameScripts_Editor.dll";
	if (CopyFileW(Utf8ToWide(path).c_str(), Utf8ToWide(copyPath).c_str(), FALSE)) loadPath = copyPath;
#endif

	s_GameScripts = LoadLibraryW(Utf8ToWide(loadPath).c_str());
	if (s_GameScripts)
	{
		Debug::Log("スクリプトを読み込みました: %s", path.c_str());
	}
	else
	{
		Debug::LogWarning("GameScripts.dll が読み込めませんでした（エラー %lu）: %s", GetLastError(), path.c_str());
	}
}

static void UnloadGameScripts()
{
	if (s_GameScripts)
	{
		ClearAllRegistries();	// DLL の中の関数を持っている登録表を先に空にする
		FreeLibrary(s_GameScripts);
		s_GameScripts = nullptr;
	}
}

//=============================================================
// 起動 → ループ → 終了
//=============================================================
int EngineMain::Run(HINSTANCE hInstance, int nCmdShow)
{
	ApplyCommandLine();

	WNDCLASSEXW wcex;
	{
		wcex.cbSize = sizeof(WNDCLASSEXW);
		wcex.style = 0;
		wcex.lpfnWndProc = WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = nullptr;
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = nullptr;
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = CLASS_NAME;
		wcex.hIconSm = nullptr;

		RegisterClassExW(&wcex);

		RECT rc = { 0, 0, (LONG)SCREEN_WIDTH, (LONG)SCREEN_HEIGHT };
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

#ifdef ENGINE_EDITOR
		std::wstring title = L"GM31 Engine - " + Utf8ToWide(EnginePaths::GetProjectDir());
#else
		std::wstring title = L"Game";
#endif
		s_Window = CreateWindowExW(0, CLASS_NAME, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
			rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
	}

	CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);

	// シーンを読む前にスクリプトを登録しておく
	LoadGameScripts();

	Manager::Init();

	ShowWindow(s_Window, nCmdShow);
	UpdateWindow(s_Window);

	// 高精度タイマーで実際の経過時間を測る
	LARGE_INTEGER freq, last, now;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&last);
	timeBeginPeriod(1);

	MSG msg;
	while (1)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				break;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			QueryPerformanceCounter(&now);
			double elapsed = double(now.QuadPart - last.QuadPart) / double(freq.QuadPart);

			if (elapsed >= 1.0 / 60.0)
			{
				last = now;

				// ウィンドウのドラッグ中などで長く止まったときに、物がワープしないよう上限をつける
				float dt = (float)elapsed;
				if (dt > 0.1f) dt = 0.1f;

				Manager::SetDeltaTime(dt);
				Manager::Update();
				Manager::Draw();
			}
		}
	}

	timeEndPeriod(1);

	// スクリプトのコンポーネントを全部消してから DLL を外す
	Manager::Uninit();
	UnloadGameScripts();

	UnregisterClassW(CLASS_NAME, wcex.hInstance);

	CoUninitialize();

	return (int)msg.wParam;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
	if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_SIZE:
		// ウィンドウの大きさに合わせて画面（バックバッファ）を作り直す
		if (wParam != SIZE_MINIMIZED) Renderer::Resize(LOWORD(lParam), HIWORD(lParam));
		break;

	case WM_INPUT:
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MOUSEHOVER:
		Input::ProcessMessage(uMsg, wParam, lParam);
		break;

	case WM_CLOSE:
		if (MessageBoxW(hWnd, L"本当に終了しますか？", L"確認", MB_OKCANCEL | MB_DEFBUTTON2) == IDOK)
		{
			DestroyWindow(hWnd);
		}
		else
		{
			return 0;
		}
	default:
		break;
	}

	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
