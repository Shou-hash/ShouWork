#pragma once
#include "Engine/Base/Common.h"

class GameScene {
public:
	GameScene() = default;
	~GameScene();

	// 初期化・更新・描画
	void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, HWND hwnd, HINSTANCE hInstance);
	void Update();
	void Draw(ID3D12GraphicsCommandList* commandList);

public:
	// 内部初期化処理
	void InitializeInput(HWND hwnd, HINSTANCE hInstance);
	void InitializeResources(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	void InitializeSphere(ID3D12Device* device);

	// ImGuiを描画
	void DrawImGui();

private:
	// ★ Key関連変数を削除し、Inputクラスのポインタを管理
	std::unique_ptr<Input> input_ = nullptr;

	// 実体ではなくポインタに変更
	DirectInput* gamePad = nullptr;

	DebugCamera debugCamera;
	Sound* soundManager = nullptr;
	Sound::SoundData soundData;
	bool isDebugCamera = false;

	// ライト
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;
	DirectionalLight* directionalLightData = nullptr;
	float uiLightDirection[3] = { 0.0f, -1.0f, 0.0f };

	// 中央モデル (Plane)
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource;
	TransformationMatrix* wvpData = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Material* materialData = nullptr;
	struct Transform transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Vector3 uvScale = { 1.0f, 1.0f, 1.0f };
	Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
	Vector3 uvTranslate = { 0.0f, 0.0f, 0.0f };
	float uiUVScale[2] = { 1.0f, 1.0f };
	float uiUVRotate = 0.0f;
	float uiUVTranslate[2] = { 0.0f, 0.0f };
	bool showModel = true;
	bool useMonsterBall = true;

	// スプライト
	Microsoft::WRL::ComPtr<ID3D12Resource> spriteMaterialResource;
	Material* spriteMaterialData = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> spriteWvpResource;
	TransformationMatrix* spriteWvpData = nullptr;
	struct Transform spriteTransform = { { 300.0f, 300.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 100.0f, 100.0f, 0.0f } };
	Vector3 spriteUVScale = { 1.0f, 1.0f, 1.0f };
	Vector3 spriteUVRotate = { 0.0f, 0.0f, 0.0f };
	Vector3 spriteUVTranslate = { 0.0f, 0.0f, 0.0f };
	float uiSpriteUVScale[2] = { 1.0f, 1.0f };
	float uiSpriteUVRotate = 0.0f;
	float uiSpriteUVTranslate[2] = { 0.0f, 0.0f };
	float uiSpritePosition[2] = { 320.0f, 180.0f };
	float uiSpriteSize[2] = { 320.0f, 180.0f };
	bool showSprite = true;
	int spriteTextureIndex = 0;

	// 球体 (Sphere)
	Microsoft::WRL::ComPtr<ID3D12Resource> sphereVertexResource;
	D3D12_VERTEX_BUFFER_VIEW sphereVertexBufferView{};
	std::vector<VertexData> sphereVertices;
	Microsoft::WRL::ComPtr<ID3D12Resource> sphereMaterialResource;
	Material* sphereMaterialData = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> sphereWvpResource;
	TransformationMatrix* sphereWvpData = nullptr;
	struct Transform sphereTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Vector3 sphereUVScale = { 1.0f, 1.0f, 1.0f };
	Vector3 sphereUVRotate = { 0.0f, 0.0f, 0.0f };
	Vector3 sphereUVTranslate = { 0.0f, 0.0f, 0.0f };
	float uiSphereUVScale[2] = { 1.0f, 1.0f };
	float uiSphereUVRotate = 0.0f;
	float uiSphereUVTranslate[2] = { 0.0f, 0.0f };
	float sphereRotate[3] = { 0.0f, 0.0f, 0.0f };
	bool showSphere = true;
	int sphereTextureIndex = 0;

	// モデル・テクスチャデータ
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU1{};
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2{};
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3{};
	std::vector<std::shared_ptr<Model>> modelEntries;
	std::shared_ptr<Model> planeModel;

	// カメラ行列
	Matrix4x4 viewMatrix;
	Matrix4x4 projectionMatrix;

	// ★【変更】全体共通変数を削除し、各描画オブジェクト用のブレンドモード変数を追加
	BlendMode planeBlendMode_ = kBlendModeNormal;  // 中央モデル (Plane) 用
	BlendMode sphereBlendMode_ = kBlendModeNormal; // 球体 (Sphere) 用
	BlendMode spriteBlendMode_ = kBlendModeNormal; // スプライト (Sprite) 用
};