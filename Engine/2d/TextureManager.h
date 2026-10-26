#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <unordered_map>
#include "externals/DirectXTex/DirectXTex.h"

class DirectXCommon;
class SrvManager;

class TextureManager {
public:
    // テクスチャ1枚分のデータ構造体
    struct TextureData {
        DirectX::TexMetadata metadata;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint32_t srvIndex;
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
    };

    // シングルトンインスタンスの取得
    static TextureManager* GetInstance();

    // 初期化処理 (DirectXCommon と SrvManager を受け取る)
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    // テクスチャの読み込み
    void LoadTexture(const std::string& filePath);

    // 既存コードとの互換用: ロードして GPU ハンドルを返す関数
    D3D12_GPU_DESCRIPTOR_HANDLE LoadTextureAndCreateSRV(const std::string& filePath);

    // 各種 getter
    const DirectX::TexMetadata& GetMetaData(const std::string& filePath);
    uint32_t GetSrvIndex(const std::string& filePath);
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

    // 静的リソース・キャッシュの解放
    void Finalize();

private:
    TextureManager() = default;
    ~TextureManager() = default;
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // 単体画像データ読み込み・リソース生成ヘルパー関数
    DirectX::ScratchImage LoadTextureFile(const std::string& filePath);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(
        ID3D12Device* device, const DirectX::TexMetadata& metadata);
    void UploadTextureData(
        ID3D12Resource* texture, const DirectX::ScratchImage& mipImages,
        ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

private:
    DirectXCommon* directXCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // テクスチャデータコンテナ (ファイルパスをキーとした std::unordered_map)
    std::unordered_map<std::string, TextureData> textureDatas_;
};