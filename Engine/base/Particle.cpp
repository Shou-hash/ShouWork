#include "Particle.h"

void Particle::Initialize(
	ID3D12Device* device,
	ID3D12DescriptorHeap* srvDescriptorHeap,
	UINT descriptorSize,
	uint32_t& srvIndexCounter,
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* dxcIncludeHandler
) {
	// 1. Particle用 RootSignature & PSO の構築
	CreateRootSignatureAndPSO(device, dxcUtils, dxcCompiler, dxcIncludeHandler);

	// 2. 頂点データ (四角形プリミティブ: 6頂点) の作成
	// ★ Common.h の VertexData 構造体 (position, u, v, normal) に合わせて u, v を個別指定に修正
	std::vector<VertexData> vertices = {
		{ {  0.5f,  0.5f, 0.0f, 1.0f }, 0.0f, 0.0f, { 0.0f, 0.0f, 1.0f } }, // 左上
		{ { -0.5f,  0.5f, 0.0f, 1.0f }, 1.0f, 0.0f, { 0.0f, 0.0f, 1.0f } }, // 右上
		{ {  0.5f, -0.5f, 0.0f, 1.0f }, 0.0f, 1.0f, { 0.0f, 0.0f, 1.0f } }, // 左下
		{ {  0.5f, -0.5f, 0.0f, 1.0f }, 0.0f, 1.0f, { 0.0f, 0.0f, 1.0f } }, // 左下
		{ { -0.5f,  0.5f, 0.0f, 1.0f }, 1.0f, 0.0f, { 0.0f, 0.0f, 1.0f } }, // 右上
		{ { -0.5f, -0.5f, 0.0f, 1.0f }, 1.0f, 1.0f, { 0.0f, 0.0f, 1.0f } }, // 右下
	};

	vertexBuffer_ = CreateBufferResource(device, sizeof(VertexData) * vertices.size());
	vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	VertexData* vertData = nullptr;
	vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertData));
	std::memcpy(vertData, vertices.data(), sizeof(VertexData) * vertices.size());

	// 3. マテリアルバッファの作成
	materialResource_ = CreateBufferResource(device, sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = 0;
	materialData_->uvTransform = MakeIdentity4x4();

	// 4. Instancing用 TransformationMatrix (StructuredBuffer) リソースの作成
	instancingResource_ = CreateBufferResource(device, sizeof(TransformationMatrix) * kNumInstance);
	instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));

	for (uint32_t index = 0; index < kNumInstance; ++index) {
		instancingData_[index].WVP = MakeIdentity4x4();
		instancingData_[index].World = MakeIdentity4x4();
	}

	// 5. Instancing用 SRV の作成
	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = kNumInstance;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(TransformationMatrix);

	instancingSrvHandleCPU_ = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSize, srvIndexCounter);
	instancingSrvHandleGPU_ = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSize, srvIndexCounter++);

	device->CreateShaderResourceView(instancingResource_.Get(), &instancingSrvDesc, instancingSrvHandleCPU_);

	// 6. Transform の初期化（ずらして配置）
	for (uint32_t index = 0; index < kNumInstance; ++index) {
		transforms_[index].scale = { 1.0f, 1.0f, 1.0f };
		transforms_[index].rotate = { 0.0f, 0.0f, 0.0f };
		transforms_[index].translate = { index * 0.1f, index * 0.1f, index * 0.1f };
	}
}

void Particle::CreateRootSignatureAndPSO(
	ID3D12Device* device,
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* dxcIncludeHandler
) {
	// Particle専用 RootSignatureの作成
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
	// rootParameters[0]: Material (Pixel CBV b0)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// rootParameters[1]: StructuredBuffer (Vertex SRV t0)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

	// rootParameters[2]: Texture (Pixel SRV t0)
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeForTexture;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForTexture);

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

	// シェーダーコンパイル
	IDxcBlob* vertexShaderBlob = CompileShader(L"Engine/Shaders/Particle.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, dxcIncludeHandler);
	IDxcBlob* pixelShaderBlob = CompileShader(L"Engine/Shaders/Particle.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, dxcIncludeHandler);

	// インプットレイアウト
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
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // 両面表示
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = UINT_MAX;

	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}

void Particle::Update(const Matrix4x4& viewProjectionMatrix) {
	// Transform情報からWVP/World行列を計算して書き込む
	for (uint32_t index = 0; index < kNumInstance; ++index) {
		Matrix4x4 worldMatrix = MakeAffineMatrix(transforms_[index].scale, transforms_[index].rotate, transforms_[index].translate);
		Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);

		instancingData_[index].WVP = worldViewProjectionMatrix;
		instancingData_[index].World = worldMatrix;
	}
}

// ★【追加】Particle用 ImGui 制御パネル
void Particle::DrawImGui() {
#ifdef USE_IMGUI
	// 1. Material & Lighting 設定
	if (ImGui::CollapsingHeader("Material & Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (materialData_) {
			ImGui::ColorEdit4("Material Color", &materialData_->color.x);
			const char* lightingTypes[] = { "None (0)", "Lambert (1)", "Half Lambert (2)" };
			int lightingType = materialData_->enableLighting;
			if (ImGui::Combo("Lighting Mode", &lightingType, lightingTypes, static_cast<int>(std::size(lightingTypes)))) {
				materialData_->enableLighting = lightingType;
			}
		}
	}

	// 2. 全インスタンス Transform 設定 (個別に調整可能)
	if (ImGui::CollapsingHeader("Instances Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
		for (uint32_t i = 0; i < kNumInstance; ++i) {
			ImGui::PushID(static_cast<int>(i));
			std::string label = std::format("Particle Instance {}", i);

			if (ImGui::TreeNode(label.c_str())) {
				ImGui::SliderFloat3("Position", &transforms_[i].translate.x, -10.0f, 10.0f);
				ImGui::SliderFloat3("Rotate", &transforms_[i].rotate.x, -3.14f, 3.14f);
				ImGui::SliderFloat3("Scale", &transforms_[i].scale.x, 0.01f, 5.0f);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}
#endif
}

void Particle::Draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU) {
	// PSO & RootSignatureのセット
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	// ルートパラメータのバインド
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU_); // Instancing用 StructuredBuffer SRV
	commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);   // uvChecker テクスチャ SRV

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// 描画 (6頂点の板ポリゴンを kNumInstance だけInstance描画)
	commandList->DrawInstanced(6, kNumInstance, 0, 0);
}