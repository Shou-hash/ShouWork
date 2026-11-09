#include "Engine/Base/Framework.h"

void Framework::Initialize()
{
	CoInitializeEx(0, COINIT_MULTITHREADED);
	SetUnhandledExceptionFilter(ExportDump);

	// WinApp
	winApp_ = std::make_unique<WinApp>();
	winApp_->Initialize();

	// DirectXCommon
	dxCommon_ = std::make_unique<DirectXCommon>();
	dxCommon_->Initialize(winApp_.get());

	// SRVマネージャ
	srvManager_ = std::make_unique<SrvManager>();
	srvManager_->Initialize(dxCommon_.get());

	// TextureManager
	TextureManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

	// ImGui(USE_IMGUI未定義のビルドでは中身が空)
	imguiManager_ = std::make_unique<ImGuiManager>();
	imguiManager_->Initialize(winApp_.get(), dxCommon_.get(), srvManager_.get());

	// DXC
	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	assert(SUCCEEDED(hr));
	hr = dxcUtils_->CreateDefaultIncludeHandler(&dxcIncludeHandler_);
	assert(SUCCEEDED(hr));

	// RootSignature・シェーダー・ブレンドPSO
	CreateRootSignature();

	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = CompileShader(L"Engine/Shaders/Object3D.VS.hlsl", L"vs_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), dxcIncludeHandler_.Get());
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = CompileShader(L"Engine/Shaders/Object3D.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), dxcIncludeHandler_.Get());

	BlendManager::GetInstance()->Initialize(
		dxCommon_->GetDevice(),
		rootSignature_.Get(),
		vertexShaderBlob.Get(),
		pixelShaderBlob.Get(),
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		DXGI_FORMAT_D24_UNORM_S8_UINT
	);
}

void Framework::Finalize()
{
	// 初期化と逆順
	imguiManager_->Finalize();
	TextureManager::GetInstance()->Finalize();
	winApp_->Finalize();
	CoUninitialize();
}

void Framework::Update()
{
	// ウィンドウが閉じられたら終了リクエスト
	if (winApp_->ProcessMessage()) {
		endRequest_ = true;
	}
}

void Framework::Run()
{
	// ゲームの初期化
	Initialize();

	while (true) {
		// 毎フレーム更新
		Update();
		// 終了リクエストが来たら抜ける
		if (IsEndRequest()) {
			break;
		}
		// 描画
		Draw();
	}

	// ゲームの終了
	Finalize();
}

void Framework::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 1;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[2].Descriptor.ShaderRegister = 0;

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));

	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}
