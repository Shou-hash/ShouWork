#pragma once

// ImGuiを使う箇所は必ずこのヘッダをインクルードする
// USE_IMGUI はプロジェクト設定で Debug / Development のみ定義(Releaseでは未定義 = 全部空になる)
#ifdef USE_IMGUI
#include "externals/DebugTxt/imgui.h"
#include "externals/DebugTxt/imgui_impl_dx12.h"
#include "externals/DebugTxt/imgui_impl_win32.h"
#endif

class WinApp;
class DirectXCommon;
class SrvManager;
struct ID3D12DescriptorHeap;

// ImGuiの管理
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
	// SrvManagerが管理するヒープ(テクスチャと同じヒープ)
	ID3D12DescriptorHeap* srvHeap_ = nullptr;
};
