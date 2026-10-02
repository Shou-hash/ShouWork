#pragma once
#include "Engine/Base/Common.h"
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

// 描画用の個別メッシュインスタンス構造体
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
};

// 1つのOBJモデル全体を表すクラス
class Model {
public:
	std::string name;
	std::vector<RenderMeshInstance> instances;

	// 簡単読み込み用静的関数
	static std::shared_ptr<Model> CreateFromOBJ(const std::string& modelName, bool smoothing = true);
};

// モデルおよびテクスチャを統合ロード管理するクラス
class ModelLoader
{
public:
	// 初期化処理（デバイスやDescriptorHeapの登録）
	static void Initialize(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		ID3D12DescriptorHeap* srvHeap,
		UINT descriptorSize,
		uint32_t& srvIndexCounter,
		D3D12_GPU_DESCRIPTOR_HANDLE defaultTexHandle
	);

	// ディレクトリとファイル名を指定してOBJをロード
	static std::shared_ptr<Model> LoadOBJ(const std::string& directoryPath, const std::string& filename, const std::string& modelName = "");

	// モデル名のみで "Resources/モデル名/モデル名.obj" または "Resources/モデル名.obj" を読み込む簡易関数
	static std::shared_ptr<Model> CreateFromOBJ(const std::string& modelName, bool smoothing = true);

	// テクスチャロード処理の内部共通関数
	static D3D12_GPU_DESCRIPTOR_HANDLE LoadTextureAndCreateSRV(const std::string& filePath);

private:
	static ID3D12Device* sDevice_;
	static ID3D12GraphicsCommandList* sCommandList_;
	static ID3D12DescriptorHeap* sSrvHeap_;
	static UINT sDescriptorSize_;
	static uint32_t sSrvIndexCounter_;
	static D3D12_GPU_DESCRIPTOR_HANDLE sDefaultTextureSrvHandleGPU_;
	static std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> sLoadedTextureResources_;
};