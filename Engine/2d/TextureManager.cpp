#include "TextureManager.h"
#include "Engine/base/ConvertString.h"
#include <cassert>
#include <filesystem>

ID3D12Device* TextureManager::sDevice_ = nullptr;
ID3D12GraphicsCommandList* TextureManager::sCommandList_ = nullptr;
ID3D12DescriptorHeap* TextureManager::sSrvHeap_ = nullptr;
UINT TextureManager::sDescriptorSize_ = 0;
uint32_t* TextureManager::sSrvIndexCounter_ = nullptr;
std::unordered_map<std::string, D3D12_GPU_DESCRIPTOR_HANDLE> TextureManager::sTextureMap_{};
std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> TextureManager::sLoadedTextureResources_{};

void TextureManager::Initialize(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    ID3D12DescriptorHeap* srvHeap,
    UINT descriptorSize,
    uint32_t& srvIndexCounter)
{
    sDevice_ = device;
    sCommandList_ = commandList;
    sSrvHeap_ = srvHeap;
    sDescriptorSize_ = descriptorSize;
    sSrvIndexCounter_ = &srvIndexCounter;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::LoadTextureAndCreateSRV(const std::string& filePath) {
    // 読み込み済み（キャッシュ存在）ならそのGPUハンドルを返す
    auto it = sTextureMap_.find(filePath);
    if (it != sTextureMap_.end()) {
        return it->second;
    }

    assert(sDevice_ && sSrvIndexCounter_ && "TextureManager::Initialize が呼出されていません。");

    DirectX::ScratchImage mTexImages = LoadTexture(filePath);
    Microsoft::WRL::ComPtr<ID3D12Resource> tResource = CreateTextureResource(sDevice_, mTexImages.GetMetadata());
    UploadTextureData(tResource.Get(), mTexImages, sDevice_, sCommandList_);

    uint32_t currentIndex = (*sSrvIndexCounter_)++;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuSrvHandle = GetGPUDescriptorHandle(sSrvHeap_, sDescriptorSize_, currentIndex);
    D3D12_CPU_DESCRIPTOR_HANDLE cpuSrvHandle = GetCPUDescriptorHandle(sSrvHeap_, sDescriptorSize_, currentIndex);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = mTexImages.GetMetadata().format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = static_cast<UINT>(mTexImages.GetMetadata().mipLevels);

    sDevice_->CreateShaderResourceView(tResource.Get(), &srvDesc, cpuSrvHandle);

    sLoadedTextureResources_.push_back(tResource);
    sTextureMap_[filePath] = gpuSrvHandle;

    return gpuSrvHandle;
}

DirectX::ScratchImage TextureManager::LoadTexture(const std::string& filePath) {
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

D3D12_CPU_DESCRIPTOR_HANDLE TextureManager::GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, UINT descriptorSize, uint32_t index) {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(descriptorSize) * index;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, UINT descriptorSize, uint32_t index) {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(descriptorSize) * index;
    return handle;
}

void TextureManager::Finalize() {
    sLoadedTextureResources_.clear();
    sTextureMap_.clear();
    sDevice_ = nullptr;
    sCommandList_ = nullptr;
    sSrvHeap_ = nullptr;
}