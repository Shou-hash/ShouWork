#pragma once
#include "Engine/Base/Common.h"
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>

class WinApp;

// =========================================================
// 1. DirectX12 ユーティリティ関数のプロトタイプ宣言
// =========================================================
ID3D12DescriptorHeap* CreateDescriptorHeap(
	ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shadervisible);

void Log(std::ostream& os, const std::string& message);
void Log(const std::string& message);

std::wstring ConvertString(const std::string& str);
std::string ConvertString(const std::wstring& str);

IDxcBlob* CompileShader(
	const std::wstring& filePath,
	const wchar_t* profile,
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler);

ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes);

DirectX::ScratchImage LoadTexture(const std::string& filePath);
ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);
ID3D12Resource* CreateDepthStenciTextureResource(ID3D12Device* device, int32_t width, int32_t height);

D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t Index);
D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t Index);

ModelData LoadObjFile(const std::string& directoryPath, const std::string& fileName);
MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& fileName);

ID3D12Resource* UploadTextureData(
	ID3D12Resource* texture,
	const DirectX::ScratchImage& mipImages,
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList);

// =========================================================
// 2. 【条件1】DirectX12 基盤クラスの定義
// =========================================================
class DirectXCommon {
public:
	// DirectX12基盤の初期化
	void Initialize(WinApp* winApp);

	// 【条件2】毎フレームの描画前処理と後処理
	void PreDraw();
	void PostDraw();

	// 【条件5】FPS固定機能
	void InitializeFixFPS();
	void UpdateFixFPS();

	// 【条件4】ComPtrから生ポインタを取り出して返すgetter群
	ID3D12Device* GetDevice() const { return device_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); } // ← 追加
	ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return srvDescriptorHeap_.Get(); }
	UINT GetDescriptorSize() const { return descriptorSize_; }
	uint32_t& GetSrvIndexCounter() { return srvIndexCounter_; }

private:
	void InitializeDevice();
	void InitializeCommand();
	void InitializeSwapChain(WinApp* winApp);
	void InitializeRenderTargets();
	void InitializeDepthBuffer(WinApp* winApp);
	void InitializeFence();

private:
	// 【条件3】生のReleaseを完全排除し、ComPtrで一括管理
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
	Microsoft::WRL::ComPtr<ID3D12Device> device_;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources_[2];
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2]{};
	UINT descriptorSize_ = 0;
	uint32_t srvIndexCounter_ = 0;

	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;

	// 【条件5】FPS固定用時刻記録
	std::chrono::steady_clock::time_point reference_;
};