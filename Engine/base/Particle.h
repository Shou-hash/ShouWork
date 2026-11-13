#pragma once
#include "Common.h"
#include <array>

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

	// ---- 以下 Particle.CS.hlsl と同じ並びで追加 ----
	uint32_t mode;
	uint32_t frame;
	uint32_t activeCount;
	uint32_t reset;
	float screenW;
	float screenH;
	float mouseX;
	float mouseY;
	float mouseRadius;
	float mouseForce;
	float mouseDown;
	float time;
	Vector4 colorA;
	Vector4 colorB;
	float sizeMin;
	float sizeMax;
	float lifeMin;
	float lifeMax;
	float speedMin;
	float speedMax;
	float wind;
	float turbulence;
	float stretch;
	float tornado;
	float mouseVelX;
	float mouseVelY;
};

// Editorで編集し、JSONに保存・ホットリロードされる演出パラメータ(モード1~4)
struct ParticlePreset {
	std::string name;
	int count;
	Vector4 background;   // 背景色(white1x1.pngに掛ける)
	Vector4 colorA;       // 粒の色A(a = 基準アルファ)
	Vector4 colorB;       // 粒の色B (AとBの間でランダム)
	float sizeMin, sizeMax;   // 横幅(px)
	float lifeMin, lifeMax;   // 寿命(秒)
	float speedMin, speedMax; // 初速(px/s)
	float wind;               // 風(px/s, +で右)
	float turbulence;         // ゆらぎの強さ
	float stretch;            // 縦/横の比(雨は縦長)
	float mouseRadius;        // マウスの影響半径(px)
	float mouseForce;         // マウスの押しのけ力
	bool additive;            // true:加算 / false:通常α
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
	~Particle();

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

	// 演出モード: 0=従来(モデル+3D) / 1=砂漠 / 2=花びら / 3=嵐の夜 / 4=雪景色
	void SetMode(int mode);
	int GetMode() const { return mode_; }
	// マウス座標は画面ピクセル(左上原点, 1280x720)
	void SetMouse(float x, float y, bool down) { mouseX_ = x; mouseY_ = y; mouseDown_ = down; }
	// 特殊演出の有効フラグ(スペースキー押下中)
	void SetTornado(bool active) { tornado_ = active; }
	Vector4 GetBackground() const;

	bool LoadPresets(const std::string& path);
	bool SavePresets(const std::string& path) const;

private:
	void CreateRootSignatureAndPSO(
		ID3D12Device* device,
		IDxcUtils* dxcUtils,
		IDxcCompiler3* dxcCompiler,
		IDxcIncludeHandler* dxcIncludeHandler
	);

	void InitDefaultPresets();
	void CheckHotReload();
	void DrawPresetEditor();

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
	bool hasDescriptors_ = false;
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
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateAlpha_; // 通常αブレンド版

	SrvManager* srvManager_ = nullptr; // SrvManagerの保持用
	bool isFirstUpdate_ = true;        // 初回バリア判定用

	static constexpr const char* kPresetPath = "particle_presets.json";
	std::array<ParticlePreset, 5> presets_{}; // [0]は未使用(従来モード)
	int mode_ = 0;
	bool reset_ = false;
	uint32_t frame_ = 0;
	float time_ = 0.0f;
	float flash_ = 0.0f; // 稲光
	float mouseX_ = -10000.0f;
	float mouseY_ = -10000.0f;
	float prevMouseX_ = -10000.0f;
	float prevMouseY_ = -10000.0f;
	bool mouseDown_ = false;
	bool tornado_ = false;
	bool hotReload_ = true;
	std::filesystem::file_time_type lastWriteTime_{};
};