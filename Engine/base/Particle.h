#pragma once
#include "Common.h"

class SrvManager;

// GPU側と完全にフォーマットを合わせた構造体
struct ParticleForGPU {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Vector4 color;
	Vector3 position;
	float currentTime;
	Vector3 velocity;
	float lifeTime;
	Vector3 scale;
	float padding;
};

// Compute Shader用定数バッファ
struct ParticleCSConstant {
	Matrix4x4 viewProjection;
	Matrix4x4 billboard;
	Vector3 emitterPos;
	float deltaTime;
	Vector3 acceleration;
	uint32_t maxParticles;
	Vector3 fieldMin;
	float pad0;
	Vector3 fieldMax;
	float pad1;
};

struct Emitter {
	struct Transform transform { { 1, 1, 1 }, { 0,0,0 }, { 0,0,0 } };
	uint32_t count = 3;
	float frequency = 0.5f;
	float frequencyTime = 0.0f;
};

struct AABB { Vector3 min; Vector3 max; };

struct AccelerationField {
	Vector3 acceleration;
	AABB area;
};

class Particle
{
public:
	// GPUパーティクル化により数万個まで拡張可能
	static const uint32_t kNumMaxInstance = 10240;

	Particle() = default;
	~Particle() = default;

	void Initialize(
		ID3D12Device* device,
		SrvManager* srvManager,
		IDxcUtils* dxcUtils,
		IDxcCompiler3* dxcCompiler,
		IDxcIncludeHandler* dxcIncludeHandler
	);

	void Update(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix);
	void Draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU);
	void DrawImGui();

private:
	void CreateRootSignatureAndPSO(
		ID3D12Device* device,
		IDxcUtils* dxcUtils,
		IDxcCompiler3* dxcCompiler,
		IDxcIncludeHandler* dxcIncludeHandler
	);

	void CreateComputePipeline(
		ID3D12Device* device,
		IDxcUtils* dxcUtils,
		IDxcCompiler3* dxcCompiler,
		IDxcIncludeHandler* dxcIncludeHandler
	);

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;

	// GPUパーティクルバッファ (UAV / SRV 兼用)
	Microsoft::WRL::ComPtr<ID3D12Resource> particleBufferResource_;
	uint32_t uavIndex_ = 0;
	uint32_t srvIndex_ = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE uavHandleGPU_{};
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};

	// Compute Shader用定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> csConstantResource_;
	ParticleCSConstant* csConstantData_ = nullptr;

	Emitter emitter_;
	AccelerationField accelerationField_{ { 15.0f, 0.0f, 0.0f }, { { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f } } };
	bool useBillboard_ = true;
	bool useField_ = true;

	// Compute パイプライン
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;

	// Graphics パイプライン
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

	SrvManager* srvManager_ = nullptr; // SrvManagerの保持用
	bool isFirstUpdate_ = true;        // 初回バリア判定用
};