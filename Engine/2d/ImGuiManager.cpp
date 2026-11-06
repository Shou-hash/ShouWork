#include "ImGuiManager.h"
#include "Engine/Base/Common.h" // ImguiCode.h(imgui本体 + impl_win32 + impl_dx12)も含まれる
#include "SrvManager.h"

void ImGuiManager::Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager)
{
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
	initInfo.SrvDescriptorHeap = dxCommon->GetSrvDescriptorHeap();

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
}

void ImGuiManager::Finalize()
{
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}
