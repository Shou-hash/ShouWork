#pragma once

// Debugビルドのみ有効。プロジェクト設定で USE_IMGUI を定義済みならそちらを優先
#if defined(_DEBUG) && !defined(USE_IMGUI)
#define USE_IMGUI
#endif

// ImGuiを使う箇所は必ずこのヘッダをインクルードする
#ifdef USE_IMGUI
#include "externals/DebugTxt/imgui.h"
#include "externals/DebugTxt/imgui_impl_dx12.h"
#include "externals/DebugTxt/imgui_impl_win32.h"
#endif

class WinApp;
class DirectXCommon;
class SrvManager;
struct ID3D12DescriptorHeap;

// ImGuiの管理(USE_IMGUI未定義なら全関数が空になる)
class ImGuiManager {
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);

	/// <summary>
	/// 終了
	/// </summary>
	void Finalize();

	/// <summary>
	/// ImGui受付開始
	/// </summary>
	void Begin();

	/// <summary>
	/// ImGui受付終了
	/// </summary>
	void End();

	/// <summary>
	/// 画面への描画
	/// </summary>
	void Draw();

private:
	DirectXCommon* dxCommon_ = nullptr;
	ID3D12DescriptorHeap* srvHeap_ = nullptr;
};
