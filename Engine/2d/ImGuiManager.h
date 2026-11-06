#pragma once

class WinApp;
class DirectXCommon;
class SrvManager;

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
};
