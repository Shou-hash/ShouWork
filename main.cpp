#include <Windows.h>
#include<cstdint>

//ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// メッセージ処理
	switch (msg)
	{
		// ウィンドウの×ボタンが押され、破棄されたときの処理
	case WM_DESTROY:
		// OSに対して、アプリケーションの終了を伝える（WM_QUITメッセージを発行）
		PostQuitMessage(0);
		return 0;
	}


	// 自分で処理しなかったメッセージは、OSの標準処理に任せる
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

//Windowsアプリケーションのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
	// 出力ウィンドウへの文字出力
	OutputDebugStringA("Hello, DirectX!\n");

	// ウィンドウクラスの設定と登録
	WNDCLASS wc{};
	// ウィンドウプロシージャを設定
	wc.lpfnWndProc = WindowProc;
	// ウィンドウクラス名
	wc.lpszClassName = L"CG2WindowClass";
	// インスタンスハンドル（WinMainの引数hInstanceをそのまま使えます）
	wc.hInstance = hInstance;
	// カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	// ウィンドウクラスの登録
	RegisterClass(&wc);

	// クライアント領域のサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;

	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };

	// クライアント領域を元に、タイトルバーなどの枠を含めた実際のウィンドウサイズを計算する
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウの作成
	HWND hwnd = CreateWindow(
		wc.lpszClassName,      // クラス名
		L"CG2",                // タイトルバーの文字
		WS_OVERLAPPEDWINDOW,   // ウィンドウスタイル
		CW_USEDEFAULT,         // 表示X座標
		CW_USEDEFAULT,         // 表示Y座標
		wrc.right - wrc.left,  // ウィンドウ横幅
		wrc.bottom - wrc.top,  // ウィンドウ縦幅
		nullptr,               // 親ウィンドウハンドル
		nullptr,               // メニューハンドル
		wc.hInstance,          // インスタンスハンドル
		nullptr);              // 追加パラメータ

	// ウィンドウの表示
	ShowWindow(hwnd, SW_SHOW);

	// メインループ
	MSG msg{};

	// ウィンドウの×ボタンが押される(WM_QUITが来る)までループ
	while (msg.message != WM_QUIT)
	{
		// ウィンドウにメッセージが来ているか確認
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else 
		{
			// ここにゲームの更新処理や描画処理を入れる
		}
	}

	return 0;
}