#pragma once
#include "Engine/Base/Common.h"
#include "Engine/Base/BlendManager.h"
#include "Engine/2d/TextureManager.h"
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <unordered_map> // ★ 追加

class DebugCamera;

struct RenderMeshInstance {
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	UINT vertexCount = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = nullptr;
	Material* materialData = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = nullptr;
	TransformationMatrix* wvpData = nullptr;

	struct Transform transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Vector3 uvScale = { 1.0f, 1.0f, 1.0f };
	Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
	Vector3 uvTranslate = { 0.0f, 0.0f, 0.0f };

	float uiUVScale[2] = { 1.0f, 1.0f };
	float uiUVRotate = 0.0f;
	float uiUVTranslate[2] = { 0.0f, 0.0f };

	int textureIndex = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE defaultSrvGpuHandle{};
	bool visible = true;

	BlendMode blendMode = kBlendModeNormal;
};

class Model {
public:
	std::string name;
	std::vector<RenderMeshInstance> instances;

	static std::shared_ptr<Model> CreateFromOBJ(const std::string& modelName, bool smoothing = true);

	static void PreDraw();
	static void PostDraw();

	void Draw(const struct Transform& transform, const DebugCamera& camera, D3D12_GPU_DESCRIPTOR_HANDLE overrideTexHandle = { 0 });
	void Draw(D3D12_GPU_DESCRIPTOR_HANDLE overrideTexHandle = { 0 });
};

class ModelLoader
{
public:
	static void Initialize(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		D3D12_GPU_DESCRIPTOR_HANDLE defaultTexHandle
	);

	static std::shared_ptr<Model> LoadOBJ(const std::string& directoryPath, const std::string& filename, const std::string& modelName = "");
	static std::shared_ptr<Model> CreateFromOBJ(const std::string& modelName, bool smoothing = true);
	static D3D12_GPU_DESCRIPTOR_HANDLE LoadTextureAndCreateSRV(const std::string& filePath);
	static void Finalize();

private:
	static ID3D12Device* sDevice_;
	static ID3D12GraphicsCommandList* sCommandList_;
	static D3D12_GPU_DESCRIPTOR_HANDLE sDefaultTextureSrvHandleGPU_;

	// 読み込み済みモデルの一括管理用キャッシュマップ
	static std::unordered_map<std::string, std::shared_ptr<Model>> sModelMap_;
};