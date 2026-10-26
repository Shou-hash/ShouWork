#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <queue>

class DirectXCommon;

class SrvManager
{
public:
	static const uint32_t kMaxSRVCount;

	void Initialize(DirectXCommon* dxCommon);

	// SRVインデックスの確保（空きリストがあれば再利用）
	uint32_t Allocate();

	// SRVインデックスの解放
	void Free(uint32_t srvIndex);

	// 確保可能かどうかチェック
	bool CanAllocate() const;

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);
	void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	void PreDraw();
	void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);

	ID3D12DescriptorHeap* GetDescriptorHeap() const { return descriptorHeap.Get(); }

private:
	DirectXCommon* directXCommon = nullptr;
	uint32_t descriptorSize = 0;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	uint32_t useIndex = 0;
	std::queue<uint32_t> freeIndices; // 解放されたインデックスを保持するキュー
};