#include "ModelLoader.h"
#include <cassert>

// 静的メンバ変数の実体定義
ID3D12Device* ModelLoader::sDevice_ = nullptr;
ID3D12GraphicsCommandList* ModelLoader::sCommandList_ = nullptr;
ID3D12DescriptorHeap* ModelLoader::sSrvHeap_ = nullptr;
UINT ModelLoader::sDescriptorSize_ = 0;
uint32_t ModelLoader::sSrvIndexCounter_ = 0;
D3D12_GPU_DESCRIPTOR_HANDLE ModelLoader::sDefaultTextureSrvHandleGPU_{};
std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> ModelLoader::sLoadedTextureResources_{};

// Model::CreateFromOBJ の実装
std::shared_ptr<Model> Model::CreateFromOBJ(const std::string& modelName, bool smoothing) {
	return ModelLoader::CreateFromOBJ(modelName, smoothing);
}

void ModelLoader::Initialize(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList,
	ID3D12DescriptorHeap* srvHeap,
	UINT descriptorSize,
	uint32_t& srvIndexCounter,
	D3D12_GPU_DESCRIPTOR_HANDLE defaultTexHandle)
{
	sDevice_ = device;
	sCommandList_ = commandList;
	sSrvHeap_ = srvHeap;
	sDescriptorSize_ = descriptorSize;
	sSrvIndexCounter_ = srvIndexCounter;
	sDefaultTextureSrvHandleGPU_ = defaultTexHandle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ModelLoader::LoadTextureAndCreateSRV(const std::string& filePath) {
	if (filePath.empty() || !std::filesystem::exists(filePath)) {
		return sDefaultTextureSrvHandleGPU_;
	}

	DirectX::ScratchImage mTexImages = LoadTexture(filePath);
	Microsoft::WRL::ComPtr<ID3D12Resource> tResource = CreateTextureResource(sDevice_, mTexImages.GetMetadata());
	UploadTextureData(tResource.Get(), mTexImages, sDevice_, sCommandList_);

	D3D12_GPU_DESCRIPTOR_HANDLE gpuSrvHandle = GetGPUDescriptorHandle(sSrvHeap_, sDescriptorSize_, sSrvIndexCounter_);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = mTexImages.GetMetadata().format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(mTexImages.GetMetadata().mipLevels);
	sDevice_->CreateShaderResourceView(tResource.Get(), &srvDesc, GetCPUDescriptorHandle(sSrvHeap_, sDescriptorSize_, sSrvIndexCounter_++));

	sLoadedTextureResources_.push_back(tResource);
	return gpuSrvHandle;
}

std::shared_ptr<Model> ModelLoader::LoadOBJ(const std::string& directoryPath, const std::string& filename, const std::string& modelName) {
	assert(sDevice_ && "ModelLoader::Initialize が呼ばれていません。");

	auto model = std::make_shared<Model>();
	model->name = modelName.empty() ? filename : modelName;

	ModelData mData = LoadObjFile(directoryPath, filename);

	for (size_t meshIdx = 0; meshIdx < mData.meshes.size(); ++meshIdx) {
		RenderMeshInstance inst;
		inst.vertexCount = static_cast<UINT>(mData.meshes[meshIdx].vertices.size());
		if (inst.vertexCount == 0) continue;

		// 頂点バッファの生成
		inst.vertexResource = CreateBufferResource(sDevice_, sizeof(VertexData) * inst.vertexCount);
		inst.vertexBufferView.BufferLocation = inst.vertexResource->GetGPUVirtualAddress();
		inst.vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * inst.vertexCount);
		inst.vertexBufferView.StrideInBytes = sizeof(VertexData);
		VertexData* vData = nullptr;
		inst.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vData));
		std::memcpy(vData, mData.meshes[meshIdx].vertices.data(), sizeof(VertexData) * inst.vertexCount);

		// マテリアルバッファの生成
		inst.materialResource = CreateBufferResource(sDevice_, sizeof(Material));
		inst.materialResource->Map(0, nullptr, reinterpret_cast<void**>(&inst.materialData));
		if (inst.materialData) {
			inst.materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			inst.materialData->enableLighting = 2;
			inst.materialData->uvTransform = MakeIdentity4x4();
		}

		// WVPバッファの生成
		inst.wvpResource = CreateBufferResource(sDevice_, sizeof(TransformationMatrix));
		inst.wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&inst.wvpData));
		if (inst.wvpData) {
			inst.wvpData->WVP = MakeIdentity4x4();
			inst.wvpData->World = MakeIdentity4x4();
		}

		// テクスチャの読み込みとSRV割り当て
		std::string texPath = mData.meshes[meshIdx].material.textureFilePath;
		inst.defaultSrvGpuHandle = LoadTextureAndCreateSRV(texPath);

		model->instances.push_back(inst);
	}

	return model;
}

std::shared_ptr<Model> ModelLoader::CreateFromOBJ(const std::string& modelName, bool smoothing) {
	// 例: "player" -> "Resources/player" 内の "player.obj" を探索、無ければ "Resources/modelName.obj"
	std::string dirPath = "Resources/" + modelName;
	std::string filePath = modelName + ".obj";

	if (!std::filesystem::exists(dirPath + "/" + filePath)) {
		dirPath = "Resources";
		filePath = modelName + ".obj";
	}

	return LoadOBJ(dirPath, filePath, modelName);
}