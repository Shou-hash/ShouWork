#pragma once
#include "Common.h"

// =========================================================
// Particle クラス（Instancing描画管理）
// =========================================================
class Particle
{
public:
	// インスタンス数（スクショ資料に従い10個）
	static const uint32_t kNumInstance = 10;

	Particle() = default;
	~Particle() = default;

	// 初期化
	void Initialize(
		ID3D12Device* device,
		ID3D12DescriptorHeap* srvDescriptorHeap,
		UINT descriptorSize,
		uint32_t& srvIndexCounter,
		IDxcUtils* dxcUtils,
		IDxcCompiler3* dxcCompiler,
		IDxcIncludeHandler* dxcIncludeHandler
	);

	// 更新
	void Update(const Matrix4x4& viewProjectionMatrix);

	// 描画
	void Draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU);

	// ★【追加】ImGui描画
	void DrawImGui();

private:
	// Particle専用の RootSignature と PipelineState の作成
	void CreateRootSignatureAndPSO(
		ID3D12Device* device,
		IDxcUtils* dxcUtils,
		IDxcCompiler3* dxcCompiler,
		IDxcIncludeHandler* dxcIncludeHandler
	);

private:
	// 頂点バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	// マテリアルバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;

	// インスタンシングバッファ (StructuredBuffer)
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
	TransformationMatrix* instancingData_ = nullptr;

	// SRV用ハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU_{};
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

	// インスタンスごとの Transform 情報
	struct Transform transforms_[kNumInstance];

	// Particle用パイプライン設定
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};