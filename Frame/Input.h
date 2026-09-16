#pragma once

#include <windows.h>

class Input
{
public:
	// マウスボタン
	enum MOUSE_BUTTON
	{
		MOUSE_LEFT,
		MOUSE_RIGHT,
		MOUSE_MIDDLE,

		MOUSE_BUTTON_MAX
	};

	// マウス座標モード
	enum MOUSE_MODE
	{
		MOUSE_MODE_ABSOLUTE,	// 絶対座標（クライアント座標）
		MOUSE_MODE_RELATIVE,	// 相対座標（視点操作向け・カーソル非表示＆ウィンドウ内に固定）
	};

private:
	// --- キーボード ---
	static BYTE m_OldKeyState[256];
	static BYTE m_KeyState[256];

	// --- マウス ---
	static bool m_OldMouseButton[MOUSE_BUTTON_MAX];
	static bool m_MouseButton[MOUSE_BUTTON_MAX];

	static int  m_MouseX;			// 絶対座標モード時のクライアント座標
	static int  m_MouseY;
	static int  m_MouseMoveX;		// 1フレームの移動量
	static int  m_MouseMoveY;
	static int  m_MouseWheel;		// 1フレームのホイール変化量
	static int  m_OldWheelValue;	// ホイール累積値の前フレーム分（差分計算用）

	static bool m_FirstUpdate;		// 初回のみ移動量を0にする

public:
	static void Init();
	static void Uninit();
	static void Update();

	// ウィンドウプロシージャから呼ぶ（マウスメッセージのフック）
	static void ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);

	// --- キーボード ---
	static bool GetKeyPress(BYTE KeyCode);
	static bool GetKeyTrigger(BYTE KeyCode);
	static bool GetKeyRelease(BYTE KeyCode);

	// --- マウスボタン ---
	static bool GetMousePress(MOUSE_BUTTON Button);
	static bool GetMouseTrigger(MOUSE_BUTTON Button);
	static bool GetMouseRelease(MOUSE_BUTTON Button);

	// --- マウス座標・移動量・ホイール ---
	static int  GetMouseX() { return m_MouseX; }
	static int  GetMouseY() { return m_MouseY; }
	static int  GetMouseMoveX() { return m_MouseMoveX; }
	static int  GetMouseMoveY() { return m_MouseMoveY; }
	static int  GetMouseWheel() { return m_MouseWheel; }
	static void ResetMouseWheel();

	// --- モード・カーソル ---
	static void SetMouseMode(MOUSE_MODE Mode);
	static void SetMouseVisible(bool Visible);
	static bool IsMouseVisible();
	static bool IsMouseConnected();
};