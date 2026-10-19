#include "ModelLoader.h"
#include "ModelDraw.h"
#include "Engine/Base/DirectXCommon.h"
#include <cassert>

ID3D12Device* ModelLoader::sDevice_ = nullptr;
ID3D12GraphicsCommandList* ModelLoader::sCommandList_ = nullptr;
D3D12_GPU_DESCRIPTOR_HANDLE ModelLoader::sDefaultTextureSrvHandleGPU_{};

// 実体定義
std::unordered_map<std::string, std::shared_ptr<Model>> ModelLoader::sModelMap_{};

std::shared_ptr<Model> Model::CreateFromOBJ(const std::string& modelName, bool smoothing) {
	return ModelLoader::CreateFromOBJ(modelName, smoothing);
}

void ModelLoader::Initialize(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList,
	D3D12_GPU_DESCRIPTOR_HANDLE defaultTexHandle)
{
	sDevice_ = device;
	sCommandList_ = commandList;
	sDefaultTextureSrvHandleGPU_ = defaultTexHandle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ModelLoader::LoadTextureAndCreateSRV(const std::string& filePath) {
	std::error_code ec;
	if (filePath.empty() || !std::filesystem::exists(filePath, ec) || ec) {
		return sDefaultTextureSrvHandleGPU_;
	}
	return TextureManager::LoadTextureAndCreateSRV(filePath);
}

std::shared_ptr<Model> ModelLoader::LoadOBJ(const std::string& directoryPath, const std::string& filename, const std::string& modelName) {
	assert(sDevice_ && "ModelLoader::Initialize が呼ばれていません。");

	// ★【一括管理】検索キーを作成
	std::string key = directoryPath + "/" + filename;

	// すでにロード済みの場合はキャッシュされているモデルを返す（ファイルの重複読み込みを防止）
	auto it = sModelMap_.find(key);
	if (it != sModelMap_.end()) {
		return it->second;
	}

	auto model = std::make_shared<Model>();
	model->name = modelName.empty() ? filename : modelName;

	ModelData mData = LoadObjFile(directoryPath, filename);

	for (size_t meshIdx = 0; meshIdx < mData.meshes.size(); ++meshIdx) {
		RenderMeshInstance inst;
		inst.vertexCount = static_cast<UINT>(mData.meshes[meshIdx].vertices.size());
		if (inst.vertexCount == 0) continue;

		// 1. 頂点バッファ生成
		inst.vertexResource = CreateBufferResource(sDevice_, sizeof(VertexData) * inst.vertexCount);
		inst.vertexBufferView.BufferLocation = inst.vertexResource->GetGPUVirtualAddress();
		inst.vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * inst.vertexCount);
		inst.vertexBufferView.StrideInBytes = sizeof(VertexData);
		VertexData* vData = nullptr;
		inst.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vData));
		std::memcpy(vData, mData.meshes[meshIdx].vertices.data(), sizeof(VertexData) * inst.vertexCount);

		// 2. マテリアルバッファ生成
		inst.materialResource = CreateBufferResource(sDevice_, sizeof(Material));
		inst.materialResource->Map(0, nullptr, reinterpret_cast<void**>(&inst.materialData));
		if (inst.materialData) {
			inst.materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			inst.materialData->enableLighting = 2;
			inst.materialData->uvTransform = MakeIdentity4x4();
		}

		// 3. WVPバッファ生成
		inst.wvpResource = CreateBufferResource(sDevice_, sizeof(TransformationMatrix));
		inst.wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&inst.wvpData));
		if (inst.wvpData) {
			inst.wvpData->WVP = MakeIdentity4x4();
			inst.wvpData->World = MakeIdentity4x4();
		}

		// 4. テクスチャ読み込み & SRV割り当て
		std::string texPath = mData.meshes[meshIdx].material.textureFilePath;
		inst.defaultSrvGpuHandle = LoadTextureAndCreateSRV(texPath);

		model->instances.push_back(inst);
	}

	// ★【一括管理】読み込んだモデルをキャッシュマップに登録
	sModelMap_[key] = model;

	return model;
}

std::shared_ptr<Model> ModelLoader::CreateFromOBJ(const std::string& modelName, bool smoothing) {
	std::string dirPath = "Resources/" + modelName;
	std::string filePath = modelName + ".obj";

	std::error_code ec;
	if (!std::filesystem::exists(dirPath + "/" + filePath, ec) || ec) {
		dirPath = "Resources";
		filePath = modelName + ".obj";
	}

	return LoadOBJ(dirPath, filePath, modelName);
}

void ModelLoader::Finalize() {
	// ★【解放処理】キャッシュを削除
	sModelMap_.clear();
	sDevice_ = nullptr;
	sCommandList_ = nullptr;
}

// --- Model クラスの描画関数実装 ---
void Model::PreDraw() {
	ModelDraw::PreDraw();
}

void Model::PostDraw() {
	ModelDraw::PostDraw();
}

void Model::Draw(const struct Transform& transform, const DebugCamera& camera, D3D12_GPU_DESCRIPTOR_HANDLE overrideTexHandle) {
	ModelDraw::Draw(this, transform, camera, overrideTexHandle);
}

void Model::Draw(D3D12_GPU_DESCRIPTOR_HANDLE overrideTexHandle) {
	ModelDraw::Draw(this, overrideTexHandle);
}