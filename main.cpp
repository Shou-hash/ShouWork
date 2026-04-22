#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <filesystem>
#include <fstream>
#include <chrono>

#pragma region 文字列の出力(stringとwstringの相互変換)

//Log関数の定義
void Log(std::ostream& os,const std::string& message) 
{
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}

// stringとwstringの相互変換関数
std::wstring ConvertString(const std::string& str);
//wstringとstringの相互変換関数
std::string ConvertString(const std::wstring& str);

#pragma endregion

#pragma region ウィンドウプロシージャの宣言

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

#pragma endregion

//Windowsアプリケーションのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
	// 出力ウィンドウへの文字出力
	OutputDebugStringA("Hello, DirectX!\n");

#pragma region 文字列の出力(stringとwstringの相互変換)

	// 文字列の出力
	std::string str0{ "STRING!!!" };

	// 数値を文字列に変換して出力
	std::string str1{ std::to_string(10) };

	// formatの使用例
	//int enemyHp = 100; // 例
	//std::string texturePath = "player.png"; // 例
	//Log(std::format("enemyHp:{},texturePath:{}\n", enemyHp, texturePath));

	// ログ出力用のディレクトリを作成
	std::filesystem::create_directories("logs");

	//現在時刻を取得
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

	//ログファイルの名前にコンマ何秒はいらないので、削って秒にする
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);

	//日本時間に変換
	std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };

	//ログファイルの名前を作成
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);

	//時刻を使ってファイル名を決定
	std::string logFilePath = "logs/" + dateString + ".log";

	//ファイルを作って書き込み準備
	std::ofstream logFile(logFilePath);

	std::wstring wstringValue = L"テスト文字列"; // 例

	// wstring->string
	Log(logFile, ConvertString(std::format(L"WSTRING: {}\n", wstringValue)));

#pragma endregion

#pragma region ウィンドウの作成

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
		L"LE2C_12_ショウ_ズーウェン",                // タイトルバーの文字
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

#pragma endregion

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

#pragma region 文字列の出力(stringとwstringの相互変換)

std::wstring ConvertString(const std::string& str) 
{
	if (str.empty()) 
	{
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) 
	{
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

std::string ConvertString(const std::wstring& str) 
{
	if (str.empty()) 
	{
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0) 
	{
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}

#pragma endregion