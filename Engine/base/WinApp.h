#pragma once
#include <Windows.h>
#include <cstdint>

// クラッシュダンプ出力関数のプロトタイプ宣言
LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception);

class WinApp {
public:
	// クラス定数の宣言（条件5）
	static constexpr int32_t kWindowWidth = 1280;
	static constexpr int32_t kWindowHeight = 720;

public:
	// 静的メンバ関数（条件3）
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public:
	WinApp() = default;
	~WinApp();

	// 通常のメンバ関数（条件3）
	void Initialize(const wchar_t* title = L"LE2C_12_ショウ_ズーウェン");
	bool ProcessMessage();
	void Finalize();

	// メンバ変数の getter（条件4）
	HWND GetHwnd() const { return hwnd_; }
	HINSTANCE GetHInstance() const { return hInstance_; }

private:
	HWND hwnd_ = nullptr;
	WNDCLASS wc_{};
	HINSTANCE hInstance_ = nullptr;
};