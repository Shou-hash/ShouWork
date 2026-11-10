#include "Particle.h"
#include "Engine/2d/ImGuiManager.h"
#include "Engine/Base/SrvManager.h"

// GPU専用の DEFAULT ヒープバッファ作成関数
static ID3D12Resource* CreateGPUBufferResource(ID3D12Device* device, size_t sizeInBytes) {
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

	ID3D12Resource* resource = nullptr;
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
}

void Particle::Update(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix) {
	// ★ Compute ShaderでDescriptorTableを使う前に、DescriptorHeapをコマンドリストにセットする
	ID3D12DescriptorHeap* descriptorHeaps[] = { srvManager_->GetDescriptorHeap() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	// 定数バッファの更新
	csConstantData_->viewProjection = Multiply(viewMatrix, projectionMatrix);

	Matrix4x4 billboard = MakeIdentity4x4();
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			billboard.m[i][j] = viewMatrix.m[j][i];
	csConstantData_->billboard = useBillboard_ ? billboard : MakeIdentity4x4();

	csConstantData_->emitterPos = emitter_.transform.translate;
	csConstantData_->deltaTime = 1.0f / 60.0f;
	csConstantData_->acceleration = accelerationField_.acceleration;
	csConstantData_->maxParticles = kNumMaxInstance;
	csConstantData_->fieldMin = useField_ ? accelerationField_.area.min : Vector3{ 1e10f, 1e10f, 1e10f };
	csConstantData_->fieldMax = useField_ ? accelerationField_.area.max : Vector3{ -1e10f, -1e10f, -1e10f };

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
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(1, srvHandleGPU_);
	commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->DrawInstanced(6, kNumMaxInstance, 0, 0);
}

void Particle::DrawImGui() {
#ifdef USE_IMGUI
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