#pragma once
#include "Common.h"

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

// クラッシュダンプの出力
LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception);