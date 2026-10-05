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

class DebugCamera; // カメラの前方宣言

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

	// ★【追加】各インスタンスごとのブレンドモード（初期値は通常α）
	BlendMode blendMode = kBlendModeNormal;
};

class Model {
public:
	std::string name;
	std::vector<RenderMeshInstance> instances;

	static std::shared_ptr<Model> CreateFromOBJ(const std::string& modelName, bool smoothing = true);

	// --- 描画用関数の追加 ---
	static void PreDraw();
	static void PostDraw();

	// 座標変換・カメラ指定で描画
	void Draw(const struct Transform& transform, const DebugCamera& camera, D3D12_GPU_DESCRIPTOR_HANDLE overrideTexHandle = { 0 });
	// 引数なし（既存の行列保持データで描画）
	void Draw(D3D12_GPU_DESCRIPTOR_HANDLE overrideTexHandle = { 0 });
};

class ModelLoader
{
public:
	// 初期化処理
	static void Initialize(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		D3D12_GPU_DESCRIPTOR_HANDLE defaultTexHandle
	);

	// OBJファイルを読み込む
	static std::shared_ptr<Model> LoadOBJ(const std::string& directoryPath, const std::string& filename, const std::string& modelName = "");

	// モデル名簡易読み込み
	static std::shared_ptr<Model> CreateFromOBJ(const std::string& modelName, bool smoothing = true);

	// テクスチャロード（TextureManagerへ委託）
	static D3D12_GPU_DESCRIPTOR_HANDLE LoadTextureAndCreateSRV(const std::string& filePath);

	// 解放処理
	static void Finalize();

private:
	static ID3D12Device* sDevice_;
	static ID3D12GraphicsCommandList* sCommandList_;
	static D3D12_GPU_DESCRIPTOR_HANDLE sDefaultTextureSrvHandleGPU_;
};