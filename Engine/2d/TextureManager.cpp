#include "TextureManager.h"
#include "Engine/Base/DirectXCommon.h"
#include "Engine/Base/SrvManager.h"
#include "Engine/Base/ConvertString.h"
#include <cassert>

TextureManager* TextureManager::GetInstance() {
    static TextureManager instance;
    return &instance;
}

void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    assert(dxCommon);
    assert(srvManager);
    directXCommon_ = dxCommon;
    srvManager_ = srvManager;
}

void TextureManager::LoadTexture(const std::string& filePath) {
    // 読み込み済みチェック (contains で検索)
    if (textureDatas_.contains(filePath)) {
        return;
    }

    assert(directXCommon_ && srvManager_ && "TextureManager::Initialize が呼び出されていません。");

    // 1. テクスチャファイルの読み込み
    DirectX::ScratchImage mipImages = LoadTextureFile(filePath);
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

    // 2. Resource の生成
    Microsoft::WRL::ComPtr<ID3D12Resource> resource = CreateTextureResource(directXCommon_->GetDevice(), metadata);

    // 3. テクスチャデータの転送
    UploadTextureData(resource.Get(), mipImages, directXCommon_->GetDevice(), directXCommon_->GetCommandList());

    // 4. テクスチャデータをマップに追加
    TextureData& textureData = textureDatas_[filePath];
    textureData.metadata = metadata;
    textureData.resource = resource;

    // SrvManager から SRV インデックスを割り当ててハンドルを取得
    textureData.srvIndex = srvManager_->Allocate();
    textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
    textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

    // SRV の生成
    srvManager_->CreateSRVforTexture2D(
        textureData.srvIndex,
        textureData.resource.Get(),
        metadata.format,
        static_cast<UINT>(metadata.mipLevels)
    );
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::LoadTextureAndCreateSRV(const std::string& filePath) {
    LoadTexture(filePath);
    return GetSrvHandleGPU(filePath);
}

const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath) {
    assert(textureDatas_.contains(filePath));
    return textureDatas_[filePath].metadata;
}

uint32_t TextureManager::GetSrvIndex(const std::string& filePath) {
    assert(textureDatas_.contains(filePath));
    return textureDatas_[filePath].srvIndex;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath) {
    assert(textureDatas_.contains(filePath));
    return textureDatas_[filePath].srvHandleGPU;
}

void TextureManager::Finalize() {
    textureDatas_.clear();
    directXCommon_ = nullptr;
    srvManager_ = nullptr;
}

DirectX::ScratchImage TextureManager::LoadTextureFile(const std::string& filePath) {
    DirectX::ScratchImage image{};
    std::wstring filePathW = ConvertString(filePath);
    HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);
    assert(SUCCEEDED(hr));

    DirectX::ScratchImage mipImages{};
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_DEFAULT, 0, mipImages);
    assert(SUCCEEDED(hr));

    return mipImages;
}

Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata) {
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = static_cast<UINT64>(metadata.width);
    resourceDesc.Height = static_cast<UINT>(metadata.height);
    resourceDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
    resourceDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
    resourceDesc.Format = metadata.format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource));

    assert(SUCCEEDED(hr));

    return resource;
}

void TextureManager::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    for (size_t mip = 0; mip < metadata.mipLevels; ++mip) {
        const DirectX::Image* img = mipImages.GetImage(mip, 0, 0);
        HRESULT hr = texture->WriteToSubresource(static_cast<UINT>(mip), nullptr, img->pixels, static_cast<UINT>(img->rowPitch), static_cast<UINT>(img->slicePitch));
        assert(SUCCEEDED(hr));
    }
}