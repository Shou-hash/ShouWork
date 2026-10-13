#pragma once
#include "Common.h"
#include <random>

// ★ GPUに転送するデータ構造体
struct ParticleForGPU {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Vector4 color;
};

// ★ Particle構造体（CPU側で動かす挙動に関するデータ）
struct ParticleData {
	struct Transform transform;
	Vector3 velocity;
	Vector4 color;
	float lifeTime;
	float currentTime;
};

// =========================================================
// Particle クラス（Instancing描画管理）
// =========================================================
class Particle
{
public:
	// ★ 最大インスタンス数（資料に従い kNumMaxInstance に変更）
	static const uint32_t kNumMaxInstance = 10;

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

	// ImGui描画
	void DrawImGui();

	// 新しい Particle を生成する関数
	ParticleData MakeNewParticle(std::mt19937& randomEngine);

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
	ParticleForGPU* instancingData_ = nullptr;

	// SRV用ハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU_{};
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

	// インスタンスごとの Particle 情報
	ParticleData particles_[kNumMaxInstance];

	// ★ 実際に描画すべきインスタンス数
	uint32_t numInstance_ = 0;

	// 乱数生成器
	std::random_device seedGenerator_;
	std::mt19937 randomEngine_;

	// Particle用パイプライン設定
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};