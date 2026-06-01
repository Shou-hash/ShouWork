#pragma once
#include "Common.h"

// ディスクリプターヒープの生成
ID3D12DescriptorHeap* CreateDescriptorHeap(
	ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shadervisible);

// ログ出力関数
void Log(std::ostream& os, const std::string& message);
void Log(const std::string& message);

// stringとwstringの相互変換関数 (プロトタイプ宣言)
std::wstring ConvertString(const std::string& str);
std::string ConvertString(const std::wstring& str);

// Shaderのコンパイル
IDxcBlob* CompileShader(
	const std::wstring& filePath,
	const wchar_t* profile,
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler);

// バッファリソースの作成
ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes);

#pragma region Texture関数

// テクスチャファイルの読み込み
DirectX::ScratchImage LoadTexture(const std::string& filePath);

// テクスチャリソースの作成
ID3D12Resource* CreateTextureresource(ID3D12Device* device, const DirectX::TexMetadata& metadata);

// テクスチャデータのアップロード
void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages);

#pragma endregion