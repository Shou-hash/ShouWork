#pragma once

// =========================================================
// 1. Windows / Standard Libraries
// =========================================================
#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <numbers>
#include <vector>
#include <memory>
#include <cassert>
#include <dbghelp.h>
#include <strsafe.h>
#include <wrl.h>

// =========================================================
// 2. DirectX / Audio / DirectInput Libraries
// =========================================================
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <xaudio2.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include "externals/DirectXTex/d3dx12.h"
#include "externals/DirectXTex/DirectXTex.h"

// =========================================================
// 3. 基本的な型・算術ヘッダーの読み込み
// =========================================================
#include "ConvertString.h"
#include "Engine/math/Matrix4x4.h"

// =========================================================
// 4. 各種構造体の定義（Engineライブラリ群より前に配置！）
// =========================================================
struct Vector4 {
	float x, y, z, w;
};

struct Vector2
{
	float x, y;
};

struct Transform
{
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct VertexData {
	Vector4 position;
	float u, v;
	Vector3 normal;
};

struct Sphere {
	Vector3 center;
	float radius;
	Vector3 rotate;
};

struct Material
{
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
};

struct TransformationMatrix
{
	Matrix4x4 WVP;
	Matrix4x4 World;
};

struct DirectionalLight
{
	Vector4 color;
	Vector3 direction;
	float intensity;
};

struct MaterialData {
	std::string textureFilePath;
};

struct MeshData
{
	std::vector<VertexData> vertices;
	MaterialData material;
};

struct ModelData
{
	std::vector<VertexData> vertices;
	MaterialData material;
	std::vector<MeshData> meshes;
};

// =========================================================
// 5. 上記の構造体を使用する Engine ライブラリ群のインクルード
// =========================================================
#include "Engine/2d/ImguiCode.h"
#include "Engine/Base/WinApp.h"
#include "Engine/Base/DirectXCommon.h"
#include "Engine/Base/ResourceObject.h"
#include "Engine/Audio/Sound.h"
#include "Engine/3d/DebugCamera.h"
#include "Engine/input/DirectInput.h"
#include "Engine/3d/ModelLoader.h"
#include "Engine/2d/TextureManager.h"
#include "Engine/3d/ModelDraw.h"
#include "Engine/Base/BlendManager.h"
#include "Engine/Input/Input.h"

// =========================================================
// 6. Pragma comments & 関数プロトタイプ宣言
// =========================================================
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dinput8.lib")

ID3D12Resource* UploadTextureData(
	ID3D12Resource* texture,
	const DirectX::ScratchImage& mipImages,
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList
);

// DXGIファクトリー (実体はmain.cpp)
extern Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;