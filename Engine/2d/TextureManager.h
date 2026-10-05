#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <unordered_map>
#include <memory>
#include "externals/DirectXTex/DirectXTex.h"

class TextureManager {
public:
    // 初期化処理
    static void Initialize(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        ID3D12DescriptorHeap* srvHeap,
        UINT descriptorSize,
        uint32_t& srvIndexCounter
    );

    // テクスチャをロードしてSRVを作成（既存のパスならキャッシュされたハンドルを返す）
    static D3D12_GPU_DESCRIPTOR_HANDLE LoadTextureAndCreateSRV(const std::string& filePath);

    // 単体画像データ読み込み・リソース生成関数群
    static DirectX::ScratchImage LoadTexture(const std::string& filePath);
    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(
        ID3D12Device* device, const DirectX::TexMetadata& metadata);
    static void UploadTextureData(
        ID3D12Resource* texture, const DirectX::ScratchImage& mipImages,
        ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

    // デスクリプタハンドル取得関数
    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, UINT descriptorSize, uint32_t index);
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, UINT descriptorSize, uint32_t index);

    // 静的リソースの解放
    static void Finalize();

private:
    static ID3D12Device* sDevice_;
    static ID3D12GraphicsCommandList* sCommandList_;
    static ID3D12DescriptorHeap* sSrvHeap_;
    static UINT sDescriptorSize_;
    static uint32_t* sSrvIndexCounter_;

    // テクスチャの重複ロードを防止するためのキャッシュ
    static std::unordered_map<std::string, D3D12_GPU_DESCRIPTOR_HANDLE> sTextureMap_;
    static std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> sLoadedTextureResources_;
};