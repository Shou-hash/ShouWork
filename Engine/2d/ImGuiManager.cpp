#include "Engine/Base/Common.h"
#include "ImGuiManager.h"
#include "SrvManager.h"

void ImGuiManager::Initialize([[maybe_unused]] WinApp* winApp, DirectXCommon* dxCommon, [[maybe_unused]] SrvManager* srvManager)
{
	dxCommon_ = dxCommon;
	srvHeap_ = dxCommon->GetSrvDescriptorHeap();

#ifdef USE_IMGUI
	// コンテキスト生成・スタイル設定
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	// Win32用初期化
	ImGui_ImplWin32_Init(winApp->GetHwnd());

	// DirectX12用初期化
	ImGui_ImplDX12_InitInfo initInfo = {};
	initInfo.Device = dxCommon->GetDevice();
	initInfo.CommandQueue = dxCommon->GetCommandQueue();
	initInfo.NumFramesInFlight = static_cast<int>(dxCommon->GetSwapChainResourcesNum());
	initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	initInfo.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	initInfo.SrvDescriptorHeap = srvHeap_;

	// SrvManagerはシングルトンではないのでUserData経由で渡す
	initInfo.UserData = srvManager;

	// SRV確保: SrvManagerからindexを確保し、対応するCPU/GPUハンドルを返す
	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) {
		SrvManager* srv = static_cast<SrvManager*>(info->UserData);
		uint32_t index = srv->Allocate();
		*out_cpu_handle = srv->GetCPUDescriptorHandle(index);
		*out_gpu_handle = srv->GetGPUDescriptorHandle(index);
		};

	// SRV解放: SrvManagerに解放機能がないので何もしない
	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {};

	ImGui_ImplDX12_Init(&initInfo);
#endif
}

void ImGuiManager::Finalize()
{
#ifdef USE_IMGUI
	// 初期化と逆順
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
}

void ImGuiManager::Begin()
{
#ifdef USE_IMGUI
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif
}

void ImGuiManager::End()
{
#ifdef USE_IMGUI
	// 描画前準備(表示データをまとめるだけ。GPUへは書かない)
	ImGui::Render();
#endif
}

void ImGuiManager::Draw()
{
#ifdef USE_IMGUI
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	// デスクリプタヒープをセットしてから描画コマンドを発行
	ID3D12DescriptorHeap* ppHeaps[] = { srvHeap_ };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}
