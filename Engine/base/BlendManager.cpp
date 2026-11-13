#include "BlendManager.h"
#include <cassert>

BlendManager* BlendManager::GetInstance() {
	static BlendManager instance;
	return &instance;
}

D3D12_BLEND_DESC BlendManager::CreateBlendDesc(BlendMode blendMode) {
	D3D12_BLEND_DESC blendDesc{};
	auto& rt = blendDesc.RenderTarget[0];
	rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// α値のブレンド設定（基本的には全モード共通）
	rt.SrcBlendAlpha = D3D12_BLEND_ONE;
	rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	rt.DestBlendAlpha = D3D12_BLEND_ZERO;

	switch (blendMode) {
	case kBlendModeNone: // ブレンドなし
		rt.BlendEnable = FALSE;
		break;

	case kBlendModeNormal: // 通常αブレンド: Src * SrcA + Dest * (1 - SrcA)
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		break;

	case kBlendModeAdd: // 加算: Src * SrcA + Dest * 1
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		rt.DestBlend = D3D12_BLEND_ONE;
		break;

	case kBlendModeSubtract: // 減算: Dest * 1 - Src * SrcA
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT; // REV_SUBTRACTで (Dest - Src)
		rt.DestBlend = D3D12_BLEND_ONE;
		break;

	case kBlendModeMultily: // 乗算: Src * 0 + Dest * Src
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_ZERO;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		rt.DestBlend = D3D12_BLEND_SRC_COLOR;
		break;

	case kBlendModeScreen: // スクリーン: Src * (1 - Dest) + Dest * 1
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		rt.DestBlend = D3D12_BLEND_ONE;
		break;

	default:
		rt.BlendEnable = FALSE;
		break;
	}

	return blendDesc;
}

void BlendManager::Initialize(
	ID3D12Device* device,
	ID3D12RootSignature* rootSignature,
	IDxcBlob* vertexShaderBlob,
	IDxcBlob* pixelShaderBlob,
	DXGI_FORMAT rtvFormat,
	DXGI_FORMAT dsvFormat)
{
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[2] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	for (int i = 0; i < kCountOfBlendMode; ++i) {
		BlendMode mode = static_cast<BlendMode>(i);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc{};
		gpsDesc.pRootSignature = rootSignature;
		gpsDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		gpsDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
		gpsDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
		gpsDesc.BlendState = CreateBlendDesc(mode);
		gpsDesc.RasterizerState = rasterizerDesc;
		gpsDesc.NumRenderTargets = 1;
		gpsDesc.RTVFormats[0] = rtvFormat;
		gpsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		gpsDesc.SampleDesc.Count = 1;
		gpsDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		gpsDesc.DepthStencilState = depthStencilDesc;
		gpsDesc.DSVFormat = dsvFormat;

		HRESULT hr = device->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(&pipelineStates_[i]));
		assert(SUCCEEDED(hr));
	}
}

void BlendManager::SetPipelineState(ID3D12GraphicsCommandList* commandList, BlendMode blendMode) {
	uint32_t index = static_cast<uint32_t>(blendMode);
	if (index < kCountOfBlendMode && pipelineStates_[index]) {
		commandList->SetPipelineState(pipelineStates_[index].Get());
	}
}