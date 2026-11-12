#define NOMINMAX
#include "Particle.h"
#include "Engine/2d/ImGuiManager.h"
#include "Engine/Base/SrvManager.h"
#include <algorithm>
#include <cstdlib>

namespace {
	json Vector4ToJson(const Vector4& v) {
		return json{ {"x", v.x}, {"y", v.y}, {"z", v.z}, {"w", v.w} };
	}

	Vector4 JsonToVector4(const json& j, const Vector4& fallback) {
		if (!j.is_object()) {
			return fallback;
		}
		return Vector4{
			j.value("x", fallback.x),
			j.value("y", fallback.y),
			j.value("z", fallback.z),
			j.value("w", fallback.w)
		};
	}

	json PresetToJson(const ParticlePreset& p) {
		return json{
			{"name", p.name},
			{"count", p.count},
			{"background", Vector4ToJson(p.background)},
			{"colorA", Vector4ToJson(p.colorA)},
			{"colorB", Vector4ToJson(p.colorB)},
			{"sizeMin", p.sizeMin},
			{"sizeMax", p.sizeMax},
			{"lifeMin", p.lifeMin},
			{"lifeMax", p.lifeMax},
			{"speedMin", p.speedMin},
			{"speedMax", p.speedMax},
			{"wind", p.wind},
			{"turbulence", p.turbulence},
			{"stretch", p.stretch},
			{"mouseRadius", p.mouseRadius},
			{"mouseForce", p.mouseForce},
			{"additive", p.additive}
		};
	}

	void JsonToPreset(const json& j, ParticlePreset& p) {
		if (!j.is_object()) {
			return;
		}
		p.name = j.value("name", p.name);
		p.count = j.value("count", p.count);
		if (j.contains("background")) p.background = JsonToVector4(j["background"], p.background);
		if (j.contains("colorA")) p.colorA = JsonToVector4(j["colorA"], p.colorA);
		if (j.contains("colorB")) p.colorB = JsonToVector4(j["colorB"], p.colorB);
		p.sizeMin = j.value("sizeMin", p.sizeMin);
		p.sizeMax = j.value("sizeMax", p.sizeMax);
		p.lifeMin = j.value("lifeMin", p.lifeMin);
		p.lifeMax = j.value("lifeMax", p.lifeMax);
		p.speedMin = j.value("speedMin", p.speedMin);
		p.speedMax = j.value("speedMax", p.speedMax);
		p.wind = j.value("wind", p.wind);
		p.turbulence = j.value("turbulence", p.turbulence);
		p.stretch = j.value("stretch", p.stretch);
		p.mouseRadius = j.value("mouseRadius", p.mouseRadius);
		p.mouseForce = j.value("mouseForce", p.mouseForce);
		p.additive = j.value("additive", p.additive);
	}
}

// GPU専用の DEFAULT ヒープバッファ作成関数
static Microsoft::WRL::ComPtr<ID3D12Resource> CreateGPUBufferResource(ID3D12Device* device, size_t sizeInBytes) {
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resDesc{};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeInBytes;
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));
	return resource;
}

Particle::~Particle() {
	if (srvManager_ && hasDescriptors_) {
		srvManager_->Free(uavIndex_);
		srvManager_->Free(srvIndex_);
		hasDescriptors_ = false;
	}
}

void Particle::Initialize(
	ID3D12Device* device,
	SrvManager* srvManager,
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* dxcIncludeHandler
) {
	srvManager_ = srvManager; // ★ SrvManagerのポインタを保持

	// 1. Graphics PSO & Compute PSO の構築
	CreateRootSignatureAndPSO(device, dxcUtils, dxcCompiler, dxcIncludeHandler);
	CreateComputePipeline(device, dxcUtils, dxcCompiler, dxcIncludeHandler);

	// 2. 板ポリゴン頂点データの作成
	std::vector<VertexData> vertices = {
		{ {  0.5f,  0.5f, 0.0f, 1.0f }, 0.0f, 0.0f, { 0.0f, 0.0f, 1.0f } },
		{ { -0.5f,  0.5f, 0.0f, 1.0f }, 1.0f, 0.0f, { 0.0f, 0.0f, 1.0f } },
		{ {  0.5f, -0.5f, 0.0f, 1.0f }, 0.0f, 1.0f, { 0.0f, 0.0f, 1.0f } },
		{ {  0.5f, -0.5f, 0.0f, 1.0f }, 0.0f, 1.0f, { 0.0f, 0.0f, 1.0f } },
		{ { -0.5f,  0.5f, 0.0f, 1.0f }, 1.0f, 0.0f, { 0.0f, 0.0f, 1.0f } },
		{ { -0.5f, -0.5f, 0.0f, 1.0f }, 1.0f, 1.0f, { 0.0f, 0.0f, 1.0f } },
	};

	vertexBuffer_ = CreateBufferResource(device, sizeof(VertexData) * vertices.size());
	vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	VertexData* vertData = nullptr;
	vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertData));
	std::memcpy(vertData, vertices.data(), sizeof(VertexData) * vertices.size());

	// 3. マテリアルバッファ作成
	materialResource_ = CreateBufferResource(device, sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = 0;
	materialData_->uvTransform = MakeIdentity4x4();

	// 4. CS定数バッファ作成
	csConstantResource_ = CreateBufferResource(device, sizeof(ParticleCSConstant));
	csConstantResource_->Map(0, nullptr, reinterpret_cast<void**>(&csConstantData_));

	// 5. GPUパーティクルバッファ (UAV / SRV) の確保
	particleBufferResource_ = CreateGPUBufferResource(device, sizeof(ParticleForGPU) * kNumMaxInstance);

	uavIndex_ = srvManager->Allocate();
	srvManager->CreateUAVforStructuredBuffer(uavIndex_, particleBufferResource_.Get(), kNumMaxInstance, sizeof(ParticleForGPU));
	uavHandleGPU_ = srvManager->GetGPUDescriptorHandle(uavIndex_);

	srvIndex_ = srvManager->Allocate();
	srvManager->CreateSRVforStructuredBuffer(srvIndex_, particleBufferResource_.Get(), kNumMaxInstance, sizeof(ParticleForGPU));
	srvHandleGPU_ = srvManager->GetGPUDescriptorHandle(srvIndex_);
	hasDescriptors_ = true;

	// 6. 演出プリセット(無ければ既定値でJSONを書き出す)
	InitDefaultPresets();
	if (!LoadPresets(kPresetPath)) {
		SavePresets(kPresetPath);
	}
	std::error_code ec;
	lastWriteTime_ = std::filesystem::last_write_time(kPresetPath, ec);
}

void Particle::CreateComputePipeline(
	ID3D12Device* device,
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* dxcIncludeHandler
) {
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // u0
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[2] = {};
	// Root[0]: CS ConstantBuffer (b0)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// Root[1]: UAV Descriptor Table (u0)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature_));
	assert(SUCCEEDED(hr));

	IDxcBlob* csBlob = CompileShader(L"Engine/Shaders/Particle.CS.hlsl", L"cs_6_0", dxcUtils, dxcCompiler, dxcIncludeHandler);
	assert(csBlob != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc{};
	computePsoDesc.pRootSignature = computeRootSignature_.Get();
	computePsoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };

	hr = device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&computePipelineState_));
	assert(SUCCEEDED(hr));
}

void Particle::CreateRootSignatureAndPSO(
	ID3D12Device* device,
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* dxcIncludeHandler
) {
	// (既存の Graphic PSO 作成処理を踏襲)
	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	descriptorRangeForInstancing[0].BaseShaderRegister = 0; // t0
	descriptorRangeForInstancing[0].NumDescriptors = 1;
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRangeForTexture[1] = {};
	descriptorRangeForTexture[0].BaseShaderRegister = 0; // t0
	descriptorRangeForTexture[0].NumDescriptors = 1;
	descriptorRangeForTexture[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForTexture[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeForTexture;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pStaticSamplers = staticSamplers;
	rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

	IDxcBlob* vertexShaderBlob = CompileShader(L"Engine/Shaders/Particle.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, dxcIncludeHandler);
	IDxcBlob* pixelShaderBlob = CompileShader(L"Engine/Shaders/Particle.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, dxcIncludeHandler);
	assert(vertexShaderBlob != nullptr && pixelShaderBlob != nullptr);

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	psoDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = UINT_MAX;

	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));

	// 通常αブレンド版(砂・花びら・雨用)
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateAlpha_));
	assert(SUCCEEDED(hr));
}

void Particle::Update(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix) {
	// 約0.5秒ごとにJSONの更新を確認(ホットリロード)
	if (hotReload_ && frame_ % 30 == 0) {
		CheckHotReload();
	}
	++frame_;
	time_ += 1.0f / 60.0f;
	// ★ Compute ShaderでDescriptorTableを使う前に、DescriptorHeapをコマンドリストにセットする
	ID3D12DescriptorHeap* descriptorHeaps[] = { srvManager_->GetDescriptorHeap() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	// 定数バッファの更新
	const int safeMode = std::clamp(mode_, 0, 3);
	const ParticlePreset& pr = presets_[safeMode];
	if (safeMode == 0) {
		csConstantData_->viewProjection = Multiply(viewMatrix, projectionMatrix);

		Matrix4x4 billboard = MakeIdentity4x4();
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
				billboard.m[i][j] = viewMatrix.m[j][i];
		csConstantData_->billboard = useBillboard_ ? billboard : MakeIdentity4x4();
	}
	else {
		// 画面ピクセル座標(左上原点)をそのままクリップ空間へ。zは0.5固定(背景は0.99)
		Matrix4x4 vp = MakeIdentity4x4();
		vp.m[0][0] = 2.0f / WinApp::kWindowWidth;
		vp.m[1][1] = -2.0f / WinApp::kWindowHeight;
		vp.m[2][2] = 0.0f;
		vp.m[3][0] = -1.0f;
		vp.m[3][1] = 1.0f;
		vp.m[3][2] = 0.5f;
		csConstantData_->viewProjection = vp;
		csConstantData_->billboard = MakeIdentity4x4();
	}

	// 稲光(嵐のみ): ランダムに光って減衰
	flash_ = (safeMode == 3) ? std::max(0.0f, flash_ - 0.04f) : 0.0f;
	if (safeMode == 3 && std::rand() % 300 == 0) {
		flash_ = 1.0f;
	}

	csConstantData_->emitterPos = emitter_.transform.translate;
	csConstantData_->deltaTime = 1.0f / 60.0f;
	csConstantData_->acceleration = accelerationField_.acceleration;
	csConstantData_->maxParticles = kNumMaxInstance;
	csConstantData_->fieldMin = useField_ ? accelerationField_.area.min : Vector3{ 1e10f, 1e10f, 1e10f };
	csConstantData_->fieldMax = useField_ ? accelerationField_.area.max : Vector3{ -1e10f, -1e10f, -1e10f };

	// モード1~3用パラメータ
	csConstantData_->mode = static_cast<uint32_t>(safeMode);
	csConstantData_->frame = frame_;
	csConstantData_->activeCount = static_cast<uint32_t>(std::clamp(pr.count, 0, static_cast<int>(kNumMaxInstance)));
	csConstantData_->reset = reset_ ? 1u : 0u;
	reset_ = false;
	csConstantData_->screenW = static_cast<float>(WinApp::kWindowWidth);
	csConstantData_->screenH = static_cast<float>(WinApp::kWindowHeight);
	csConstantData_->mouseX = mouseX_;
	csConstantData_->mouseY = mouseY_;
	csConstantData_->mouseRadius = pr.mouseRadius;
	csConstantData_->mouseForce = pr.mouseForce;
	csConstantData_->mouseDown = mouseDown_ ? 1.0f : 0.0f;
	csConstantData_->time = time_;
	csConstantData_->colorA = pr.colorA;
	csConstantData_->colorB = pr.colorB;
	csConstantData_->sizeMin = pr.sizeMin;
	csConstantData_->sizeMax = std::max(pr.sizeMax, pr.sizeMin);
	csConstantData_->lifeMin = std::max(pr.lifeMin, 0.1f); // 0だと毎フレーム作り直しになる
	csConstantData_->lifeMax = std::max(pr.lifeMax, csConstantData_->lifeMin);
	csConstantData_->speedMin = pr.speedMin;
	csConstantData_->speedMax = std::max(pr.speedMax, pr.speedMin);
	csConstantData_->wind = pr.wind;
	csConstantData_->turbulence = pr.turbulence;
	csConstantData_->stretch = pr.stretch;

	// 1. リソースバリア: 初回は COMMON から UNORDERED_ACCESS へ遷移させる
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = particleBufferResource_.Get();
	barrier.Transition.StateBefore = isFirstUpdate_ ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	isFirstUpdate_ = false; // 初回フラグを解除

	// 2. Compute Shader の実行 (Dispatch)
	commandList->SetComputeRootSignature(computeRootSignature_.Get());
	commandList->SetPipelineState(computePipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, csConstantResource_->GetGPUVirtualAddress());
	commandList->SetComputeRootDescriptorTable(1, uavHandleGPU_);

	uint32_t numThreadGroups = (kNumMaxInstance + 1023) / 1024;
	commandList->Dispatch(numThreadGroups, 1, 1);

	// 3. リソースバリア: UNORDERED_ACCESS -> NON_PIXEL_SHADER_RESOURCE (Vertex Shader読み取り用)
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(1, &barrier);
}

void Particle::Draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU) {
	const int safeMode = std::clamp(mode_, 0, 3);
	const bool additive = (safeMode == 0) || presets_[safeMode].additive;
	commandList->SetPipelineState(additive ? pipelineState_.Get() : pipelineStateAlpha_.Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(1, srvHandleGPU_);
	commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->DrawInstanced(6, kNumMaxInstance, 0, 0);
}

void Particle::DrawImGui() {
#ifdef USE_IMGUI
	if (mode_ != 0) {
		DrawPresetEditor();
		return;
	}
	ImGui::Checkbox("useBillboard", &useBillboard_);
	if (ImGui::CollapsingHeader("Emitter", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("EmitterTranslate", &emitter_.transform.translate.x, 0.01f, -100.0f, 100.0f);
	}
	if (ImGui::CollapsingHeader("Acceleration Field", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("useField", &useField_);
		ImGui::DragFloat3("Acceleration", &accelerationField_.acceleration.x, 0.1f);
		ImGui::DragFloat3("Area Min", &accelerationField_.area.min.x, 0.01f);
		ImGui::DragFloat3("Area Max", &accelerationField_.area.max.x, 0.01f);
	}
#endif
}

void Particle::SetMode(int mode) {
	mode = std::clamp(mode, 0, 3);
	if (mode == mode_) {
		return;
	}
	mode_ = mode;
	reset_ = true; // 全粒を作り直す
}

Vector4 Particle::GetBackground() const {
	const int safeMode = std::clamp(mode_, 0, 3);
	Vector4 c = presets_[safeMode].background;
	if (safeMode == 1) {
		// 砂嵐の突風に合わせて大気の霞み・明暗をわずかに揺らす
		float haze = 0.03f * std::sin(time_ * 0.85f) + 0.015f * std::sin(time_ * 2.13f);
		c.x = std::clamp(c.x + haze, 0.0f, 1.0f);
		c.y = std::clamp(c.y + haze * 0.85f, 0.0f, 1.0f);
		c.z = std::clamp(c.z + haze * 0.6f, 0.0f, 1.0f);
	}
	c.x += flash_ * 0.6f;
	c.y += flash_ * 0.6f;
	c.z += flash_ * 0.7f;
	return c;
}

void Particle::InitDefaultPresets() {
	//                 name            count  background                    colorA                         colorB                         size          life         speed           wind    turb   stretch mRadius mForce   additive
	presets_[1] = { "Desert",        7500, { 0.76f, 0.58f, 0.35f, 1.0f }, { 0.74f, 0.52f, 0.29f, 0.42f }, { 0.97f, 0.84f, 0.62f, 0.72f }, 16.0f, 190.0f, 8.0f, 16.0f, 340.0f, 780.0f, 460.0f, 150.0f, 1.0f, 190.0f, 7500.0f, false };
	presets_[2] = { "Spring Petals", 1200, { 0.55f, 0.78f, 0.95f, 1.0f }, { 1.00f, 0.72f, 0.80f, 0.95f }, { 1.00f, 0.88f, 0.92f, 1.00f }, 12.0f,  30.0f, 12.0f, 20.0f,  60.0f, 140.0f,  90.0f,  70.0f, 0.65f, 180.0f, 3500.0f, false };
	presets_[3] = { "Night Storm",   6000, { 0.03f, 0.04f, 0.09f, 1.0f }, { 0.55f, 0.65f, 0.85f, 0.55f }, { 0.80f, 0.85f, 1.00f, 0.90f },  2.0f,   4.5f, 20.0f, 20.0f, 1100.0f, 1700.0f, 120.0f,  60.0f, 14.0f, 200.0f,    0.0f, false };
}

bool Particle::LoadPresets(const std::string& path) {
	std::ifstream file(path);
	if (!file.is_open()) {
		return false;
	}
	try {
		json j;
		file >> j;
		if (!j.contains("presets") || !j["presets"].is_array()) {
			return false;
		}
		const auto& arr = j["presets"];
		for (size_t i = 0; i < arr.size() && i < 3; ++i) {
			JsonToPreset(arr[i], presets_[i + 1]);
		}
	}
	catch (...) {
		return false; // 編集途中などの壊れたJSONは無視して現在値を維持
	}
	return true;
}

bool Particle::SavePresets(const std::string& path) const {
	try {
		json arr = json::array();
		for (size_t i = 1; i < presets_.size(); ++i) {
			arr.push_back(PresetToJson(presets_[i]));
		}
		json j;
		j["presets"] = arr;

		std::ofstream file(path);
		if (!file.is_open()) {
			return false;
		}
		file << j.dump(4);
	}
	catch (...) {
		return false;
	}
	return true;
}

void Particle::CheckHotReload() {
	std::error_code ec;
	auto t = std::filesystem::last_write_time(kPresetPath, ec);
	if (ec || t == lastWriteTime_) {
		return;
	}
	// 読めなかった場合は時刻を更新せず、次回また試す
	if (LoadPresets(kPresetPath)) {
		lastWriteTime_ = t;
	}
}

void Particle::DrawPresetEditor() {
#ifdef USE_IMGUI
	ParticlePreset& p = presets_[mode_];
	ImGui::Text("Preset: %s", p.name.c_str());
	ImGui::SliderInt("Count", &p.count, 0, static_cast<int>(kNumMaxInstance));
	ImGui::ColorEdit4("Background", &p.background.x);
	ImGui::ColorEdit4("Color A", &p.colorA.x);
	ImGui::ColorEdit4("Color B", &p.colorB.x);
	ImGui::DragFloatRange2("Size (px)", &p.sizeMin, &p.sizeMax, 0.5f, 0.5f, 600.0f);
	ImGui::DragFloatRange2("Life (s)", &p.lifeMin, &p.lifeMax, 0.1f, 0.1f, 60.0f);
	ImGui::DragFloatRange2("Speed", &p.speedMin, &p.speedMax, 5.0f, 0.0f, 3000.0f);
	ImGui::DragFloat("Wind", &p.wind, 5.0f, -2000.0f, 2000.0f);
	ImGui::DragFloat("Turbulence", &p.turbulence, 1.0f, 0.0f, 1000.0f);
	ImGui::DragFloat("Stretch (H/W)", &p.stretch, 0.05f, 0.1f, 30.0f);
	ImGui::DragFloat("Mouse Radius", &p.mouseRadius, 1.0f, 0.0f, 600.0f);
	ImGui::DragFloat("Mouse Force", &p.mouseForce, 50.0f, 0.0f, 30000.0f);
	ImGui::Checkbox("Additive Blend", &p.additive);
	ImGui::Separator();
	if (ImGui::Button("Save Presets")) {
		SavePresets(kPresetPath);
		std::error_code ec;
		lastWriteTime_ = std::filesystem::last_write_time(kPresetPath, ec); // 自分の保存を再読込しない
	}
	ImGui::SameLine();
	if (ImGui::Button("Load Presets")) {
		LoadPresets(kPresetPath);
	}
	ImGui::SameLine();
	if (ImGui::Button("Restart")) {
		reset_ = true;
	}
	ImGui::Checkbox("Hot Reload (particle_presets.json)", &hotReload_);
#endif
}