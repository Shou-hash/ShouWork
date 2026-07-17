#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dbghelp.h>
#include <strsafe.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <xaudio2.h>
#include <fstream>

#include "ConvertString.h"
#include "Matrix4x4.h"
#include "ImguiCode.h"
#include "externals/DirectXTex/DirectXTex.h"

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

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

// 頂点データ構造体の定義をトップレベルに移動
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

struct ModelData
{
	std::vector<VertexData> vertices;
	MaterialData material;
};

// DXGIファクトリー (実体はmain.cpp)
extern IDXGIFactory7* dxgiFactory;