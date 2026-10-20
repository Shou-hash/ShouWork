#include "WinApp.h"
#include <dbghelp.h>
#include <strsafe.h>

#pragma comment(lib, "Dbghelp.lib")

#ifdef Debug
#include "externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif // Debug

// クラッシュダンプの出力関数定義
LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);

	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
		time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);

	HANDLE dumpFileHandle = CreateFileW(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();

	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{};
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;

	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

	CloseHandle(dumpFileHandle);
	return EXCEPTION_EXECUTE_HANDLER;
}

// 静的メンバ関数の実装
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

#ifdef Debug
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif

	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

WinApp::~WinApp() {
	Finalize();
}

void WinApp::Initialize(const wchar_t* title) {
	hInstance_ = GetModuleHandle(nullptr);

	wc_.lpfnWndProc = WindowProc;
	wc_.lpszClassName = L"CG2WindowClass";
	wc_.hInstance = hInstance_;
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc_);

	RECT wrc = { 0, 0, kWindowWidth, kWindowHeight };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	hwnd_ = CreateWindow(
		wc_.lpszClassName,
		title,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		hInstance_,
		nullptr);

	ShowWindow(hwnd_, SW_SHOW);
}

bool WinApp::ProcessMessage() {
	MSG msg{};
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.message == WM_QUIT;
}

void WinApp::Finalize() {
	if (hwnd_) {
		CloseWindow(hwnd_);
		hwnd_ = nullptr;
	}
	if (hInstance_) {
		UnregisterClass(wc_.lpszClassName, hInstance_);
		hInstance_ = nullptr;
	}
}