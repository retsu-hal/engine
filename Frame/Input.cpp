
#include "main.h"
#include "Input.h"

#include <windowsx.h>


//======================================================================
// マウス内部実装（このファイルの外からは見えない）
//======================================================================
namespace
{
	// マウスの生の状態
	struct RawMouseState
	{
		bool leftButton;
		bool middleButton;
		bool rightButton;
		bool xButton1;
		bool xButton2;
		int  x;
		int  y;
		int  scrollWheelValue;
		Input::MOUSE_MODE positionMode;
	};

	RawMouseState     s_State = {};
	HWND              s_Window = NULL;
	Input::MOUSE_MODE s_Mode = Input::MOUSE_MODE_ABSOLUTE;

	HANDLE s_ScrollWheelValue = NULL;
	HANDLE s_RelativeRead = NULL;
	HANDLE s_AbsoluteMode = NULL;
	HANDLE s_RelativeMode = NULL;

	int  s_LastX = 0;
	int  s_LastY = 0;
	int  s_RelativeX = INT32_MAX;
	int  s_RelativeY = INT32_MAX;
	bool s_InFocus = true;


	void SafeCloseHandle(HANDLE& handle)
	{
		if (handle)
		{
			CloseHandle(handle);
			handle = NULL;
		}
	}

	// カーソルをウィンドウのクライアント領域に閉じ込める
	void ClipToWindow()
	{
		assert(s_Window != NULL);

		RECT rect;
		GetClientRect(s_Window, &rect);

		POINT ul;
		ul.x = rect.left;
		ul.y = rect.top;

		POINT lr;
		lr.x = rect.right;
		lr.y = rect.bottom;

		MapWindowPoints(s_Window, NULL, &ul, 1);
		MapWindowPoints(s_Window, NULL, &lr, 1);

		rect.left = ul.x;
		rect.top = ul.y;
		rect.right = lr.x;
		rect.bottom = lr.y;

		ClipCursor(&rect);
	}

	// マウスモジュールの初期化
	void MouseInitialize(HWND window)
	{
		memset(&s_State, 0, sizeof(s_State));

		assert(window != NULL);

		RAWINPUTDEVICE rid;
		rid.usUsagePage = 0x01;	// HID_USAGE_PAGE_GENERIC
		rid.usUsage = 0x02;		// HID_USAGE_GENERIC_MOUSE
		rid.dwFlags = RIDEV_INPUTSINK;
		rid.hwndTarget = window;
		RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));

		s_Window = window;
		s_Mode = Input::MOUSE_MODE_ABSOLUTE;

		if (!s_ScrollWheelValue) { s_ScrollWheelValue = CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE); }
		if (!s_RelativeRead) { s_RelativeRead = CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE); }
		if (!s_AbsoluteMode) { s_AbsoluteMode = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE); }
		if (!s_RelativeMode) { s_RelativeMode = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE); }

		s_LastX = 0;
		s_LastY = 0;
		s_RelativeX = INT32_MAX;
		s_RelativeY = INT32_MAX;
		s_InFocus = true;
	}

	// マウスモジュールの終了処理
	void MouseFinalize()
	{
		SafeCloseHandle(s_ScrollWheelValue);
		SafeCloseHandle(s_RelativeRead);
		SafeCloseHandle(s_AbsoluteMode);
		SafeCloseHandle(s_RelativeMode);
	}

	// 現在のマウス状態を取り出す
	void MouseGetState(RawMouseState* pState)
	{
		memcpy(pState, &s_State, sizeof(s_State));
		pState->positionMode = s_Mode;

		DWORD result = WaitForSingleObjectEx(s_ScrollWheelValue, 0, FALSE);
		if (result == WAIT_FAILED) { return; }

		if (result == WAIT_OBJECT_0)
		{
			pState->scrollWheelValue = 0;
		}

		if (pState->positionMode == Input::MOUSE_MODE_RELATIVE)
		{
			result = WaitForSingleObjectEx(s_RelativeRead, 0, FALSE);
			if (result == WAIT_FAILED) { return; }

			if (result == WAIT_OBJECT_0)
			{
				// 前回の読み取り以降、移動していない
				pState->x = 0;
				pState->y = 0;
			}
			else
			{
				SetEvent(s_RelativeRead);
			}
		}
	}

	// マウスの座標モードを設定する
	void MouseSetMode(Input::MOUSE_MODE mode)
	{
		if (s_Mode == mode)
			return;

		SetEvent((mode == Input::MOUSE_MODE_ABSOLUTE) ? s_AbsoluteMode : s_RelativeMode);

		assert(s_Window != NULL);

		TRACKMOUSEEVENT tme;
		tme.cbSize = sizeof(tme);
		tme.dwFlags = TME_HOVER;
		tme.hwndTrack = s_Window;
		tme.dwHoverTime = 1;
		TrackMouseEvent(&tme);
	}

	// ウィンドウメッセージの処理
	void MouseProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
	{
		HANDLE evts[3] =
		{
			s_ScrollWheelValue,
			s_AbsoluteMode,
			s_RelativeMode
		};

		switch (WaitForMultipleObjectsEx(_countof(evts), evts, FALSE, 0, FALSE))
		{
		case WAIT_OBJECT_0:
			s_State.scrollWheelValue = 0;
			ResetEvent(evts[0]);
			break;

		case (WAIT_OBJECT_0 + 1):
		{
			s_Mode = Input::MOUSE_MODE_ABSOLUTE;
			ClipCursor(nullptr);

			POINT point;
			point.x = s_LastX;
			point.y = s_LastY;

			// リモートデスクトップに対応するため、移動前にカーソルを表示する
			ShowCursor(TRUE);

			if (MapWindowPoints(s_Window, nullptr, &point, 1))
			{
				SetCursorPos(point.x, point.y);
			}

			s_State.x = s_LastX;
			s_State.y = s_LastY;
		}
		break;

		case (WAIT_OBJECT_0 + 2):
		{
			ResetEvent(s_RelativeRead);

			s_Mode = Input::MOUSE_MODE_RELATIVE;
			s_State.x = s_State.y = 0;
			s_RelativeX = INT32_MAX;
			s_RelativeY = INT32_MAX;

			ShowCursor(FALSE);

			ClipToWindow();
		}
		break;

		case WAIT_FAILED:
			return;
		}

		switch (message)
		{
		case WM_ACTIVATEAPP:
			if (wParam)
			{
				s_InFocus = true;

				if (s_Mode == Input::MOUSE_MODE_RELATIVE)
				{
					s_State.x = s_State.y = 0;
					ShowCursor(FALSE);
					ClipToWindow();
				}
			}
			else
			{
				int scrollWheel = s_State.scrollWheelValue;
				memset(&s_State, 0, sizeof(s_State));
				s_State.scrollWheelValue = scrollWheel;
				s_InFocus = false;
			}
			return;

		case WM_INPUT:
			if (s_InFocus && s_Mode == Input::MOUSE_MODE_RELATIVE)
			{
				RAWINPUT raw;
				UINT rawSize = sizeof(raw);

				GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &rawSize, sizeof(RAWINPUTHEADER));

				if (raw.header.dwType == RIM_TYPEMOUSE)
				{
					if (!(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
					{
						s_State.x = raw.data.mouse.lLastX;
						s_State.y = raw.data.mouse.lLastY;

						ResetEvent(s_RelativeRead);
					}
					else if (raw.data.mouse.usFlags & MOUSE_VIRTUAL_DESKTOP)
					{
						// リモートデスクトップなどに対応
						const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
						const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

						int x = (int)((raw.data.mouse.lLastX / 65535.0f) * width);
						int y = (int)((raw.data.mouse.lLastY / 65535.0f) * height);

						if (s_RelativeX == INT32_MAX)
						{
							s_State.x = s_State.y = 0;
						}
						else
						{
							s_State.x = x - s_RelativeX;
							s_State.y = y - s_RelativeY;
						}

						s_RelativeX = x;
						s_RelativeY = y;

						ResetEvent(s_RelativeRead);
					}
				}
			}
			return;

		case WM_MOUSEMOVE:
			break;

		case WM_LBUTTONDOWN:
			s_State.leftButton = true;
			break;

		case WM_LBUTTONUP:
			s_State.leftButton = false;
			break;

		case WM_RBUTTONDOWN:
			s_State.rightButton = true;
			break;

		case WM_RBUTTONUP:
			s_State.rightButton = false;
			break;

		case WM_MBUTTONDOWN:
			s_State.middleButton = true;
			break;

		case WM_MBUTTONUP:
			s_State.middleButton = false;
			break;

		case WM_MOUSEWHEEL:
			s_State.scrollWheelValue += GET_WHEEL_DELTA_WPARAM(wParam);
			return;

		case WM_XBUTTONDOWN:
			switch (GET_XBUTTON_WPARAM(wParam))
			{
			case XBUTTON1: s_State.xButton1 = true; break;
			case XBUTTON2: s_State.xButton2 = true; break;
			}
			break;

		case WM_XBUTTONUP:
			switch (GET_XBUTTON_WPARAM(wParam))
			{
			case XBUTTON1: s_State.xButton1 = false; break;
			case XBUTTON2: s_State.xButton2 = false; break;
			}
			break;

		case WM_MOUSEHOVER:
			break;

		default:
			// マウスに対するメッセージは無かった…
			return;
		}

		if (s_Mode == Input::MOUSE_MODE_ABSOLUTE)
		{
			// すべてのマウスメッセージに対して新しい座標を取得する
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);

			s_State.x = s_LastX = xPos;
			s_State.y = s_LastY = yPos;
		}
	}

} // namespace


//======================================================================
// static メンバー変数
//======================================================================
BYTE Input::m_OldKeyState[256];
BYTE Input::m_KeyState[256];

bool Input::m_OldMouseButton[Input::MOUSE_BUTTON_MAX];
bool Input::m_MouseButton[Input::MOUSE_BUTTON_MAX];

int  Input::m_MouseX = 0;
int  Input::m_MouseY = 0;
int  Input::m_MouseMoveX = 0;
int  Input::m_MouseMoveY = 0;
int  Input::m_MouseWheel = 0;
int  Input::m_OldWheelValue = 0;

bool Input::m_FirstUpdate = true;


//======================================================================
// 初期化・終了・更新
//======================================================================
void Input::Init()
{
	memset(m_OldKeyState, 0, 256);
	memset(m_KeyState, 0, 256);

	memset(m_OldMouseButton, 0, sizeof(m_OldMouseButton));
	memset(m_MouseButton, 0, sizeof(m_MouseButton));

	m_MouseX = m_MouseY = 0;
	m_MouseMoveX = m_MouseMoveY = 0;
	m_MouseWheel = 0;
	m_OldWheelValue = 0;
	m_FirstUpdate = true;

	// ウィンドウ生成後に呼ばれる前提
	MouseInitialize(GetWindow());
}


void Input::Uninit()
{
	MouseFinalize();
}


void Input::Update()
{
	//----------------------------------------
	// キーボード
	//----------------------------------------
	memcpy(m_OldKeyState, m_KeyState, 256);

	if (!GetKeyboardState(m_KeyState))
	{
		// 取得失敗時（フォーカス喪失時など）は全キー離した扱いにする
		memset(m_KeyState, 0, 256);
	}

	//----------------------------------------
	// マウス
	//----------------------------------------
	memcpy(m_OldMouseButton, m_MouseButton, sizeof(m_MouseButton));

	RawMouseState state{};
	MouseGetState(&state);

	m_MouseButton[MOUSE_LEFT] = state.leftButton;
	m_MouseButton[MOUSE_RIGHT] = state.rightButton;
	m_MouseButton[MOUSE_MIDDLE] = state.middleButton;


	if (state.positionMode == MOUSE_MODE_ABSOLUTE)
	{
		// 絶対座標モード：前フレームとの差が移動量
		m_MouseMoveX = m_FirstUpdate ? 0 : state.x - m_MouseX;
		m_MouseMoveY = m_FirstUpdate ? 0 : state.y - m_MouseY;

		m_MouseX = state.x;
		m_MouseY = state.y;
	}
	else
	{
		// 相対座標モード：state.x / y がそのまま移動量
		m_MouseMoveX = state.x;
		m_MouseMoveY = state.y;
	}

	// ホイールは累積値なので差分を取る
	m_MouseWheel = m_FirstUpdate ? 0 : state.scrollWheelValue - m_OldWheelValue;
	m_OldWheelValue = state.scrollWheelValue;

	m_FirstUpdate = false;
}


void Input::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	MouseProcessMessage(message, wParam, lParam);
}


//======================================================================
// キーボード
//======================================================================
bool Input::GetKeyPress(BYTE KeyCode)
{
	return (m_KeyState[KeyCode] & 0x80) != 0;
}

bool Input::GetKeyTrigger(BYTE KeyCode)
{
	return ((m_KeyState[KeyCode] & 0x80) && !(m_OldKeyState[KeyCode] & 0x80));
}

bool Input::GetKeyRelease(BYTE KeyCode)
{
	return (!(m_KeyState[KeyCode] & 0x80) && (m_OldKeyState[KeyCode] & 0x80));
}


//======================================================================
// マウスボタン
//======================================================================
bool Input::GetMousePress(MOUSE_BUTTON Button)
{
	return m_MouseButton[Button];
}

bool Input::GetMouseTrigger(MOUSE_BUTTON Button)
{
	return (m_MouseButton[Button] && !m_OldMouseButton[Button]);
}

bool Input::GetMouseRelease(MOUSE_BUTTON Button)
{
	return (!m_MouseButton[Button] && m_OldMouseButton[Button]);
}


//======================================================================
// その他
//======================================================================
void Input::ResetMouseWheel()
{
	SetEvent(s_ScrollWheelValue);
	m_OldWheelValue = 0;
	m_MouseWheel = 0;
}

void Input::SetMouseMode(MOUSE_MODE Mode)
{
	MouseSetMode(Mode);
}

void Input::SetMouseVisible(bool Visible)
{
	if (s_Mode == MOUSE_MODE_RELATIVE)
		return;

	CURSORINFO info = { sizeof(CURSORINFO), 0, nullptr, {} };
	GetCursorInfo(&info);

	bool isVisible = (info.flags & CURSOR_SHOWING) != 0;

	if (isVisible != Visible)
	{
		ShowCursor(Visible);
	}
}

bool Input::IsMouseVisible()
{
	if (s_Mode == MOUSE_MODE_RELATIVE)
		return false;

	CURSORINFO info = { sizeof(CURSORINFO), 0, nullptr, {} };
	GetCursorInfo(&info);

	return (info.flags & CURSOR_SHOWING) != 0;
}

bool Input::IsMouseConnected()
{
	return GetSystemMetrics(SM_MOUSEPRESENT) != 0;
}