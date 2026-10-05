#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <array>

// スライド資料に基づくBlendMode定義
enum BlendMode {
	//!< ブレンドなし
	kBlendModeNone,
	//!< 通常αブレンド。デフォルト。Src * SrcA + Dest * (1 - SrcA)
	kBlendModeNormal,
	//!< 加算。Src * SrcA + Dest * 1
	kBlendModeAdd,
	//!< 減算。Dest * 1 - Src * SrcA
	kBlendModeSubtract,
	//!< 乗算。Src * 0 + Dest * Src
	kBlendModeMultily,
	//!< スクリーン。Src * (1 - Dest) + Dest * 1
	kBlendModeScreen,
	// 利用してはいけない
	kCountOfBlendMode,
};

class BlendManager {
public:
	static BlendManager* GetInstance();

	// 各ブレンドモードの設定（D3D12_BLEND_DESC）を生成する関数
	static D3D12_BLEND_DESC CreateBlendDesc(BlendMode blendMode);

	// 全ブレンドモードのPSOを生成して初期化
	void Initialize(
		ID3D12Device* device,
		ID3D12RootSignature* rootSignature,
		IDxcBlob* vertexShaderBlob,
		IDxcBlob* pixelShaderBlob,
		DXGI_FORMAT rtvFormat,
		DXGI_FORMAT dsvFormat);

	// コマンドリストに指定したブレンドモードのPipeline Stateをセット
	void SetPipelineState(ID3D12GraphicsCommandList* commandList, BlendMode blendMode);

private:
	BlendManager() = default;
	~BlendManager() = default;

	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, kCountOfBlendMode> pipelineStates_;
};