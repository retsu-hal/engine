#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include "EngineAPI.h"
#include <stdio.h>

#define NOMINMAX
#include <windows.h>
#include <assert.h>
#include <functional>
#include <list>
#include <vector>

#include <d3d11.h>
#pragma comment (lib, "d3d11.lib")

#include "Input.h"

#include <DirectXMath.h>
using namespace DirectX;


#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"




#pragma comment (lib, "winmm.lib")


#define SCREEN_WIDTH	(1280)
#define SCREEN_HEIGHT	(720)


ENGINE_API HWND GetWindow();	// 定義は EngineMain.cpp

void Invoke(std::function<void()> Function, int Time);

