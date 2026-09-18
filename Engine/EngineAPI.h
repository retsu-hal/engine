#pragma once

//=============================================================
// Engine.dll の関数・クラスを外（Editor.exe / GameScripts.dll）から使えるようにする印
//   Engine プロジェクトでは ENGINE_EXPORTS を定義する → dllexport（出す側）
//   それ以外のプロジェクトでは                        → dllimport（使う側）
//=============================================================
#ifdef ENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif

// STL を持つクラスを DLL から出すときの警告（同じコンパイラ・同じ設定なら問題ない）
#pragma warning(disable : 4251 4275)
