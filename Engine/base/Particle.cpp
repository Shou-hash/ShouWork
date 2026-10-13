#include "Particle.h"

// 新しい Particle を生成する関数
ParticleData Particle::MakeNewParticle(std::mt19937& randomEngine) {
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
	std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

	ParticleData particle;
	particle.transform.scale = { 1.0f, 1.0f, 1.0f };
	particle.transform.rotate = { 0.0f, 0.0f, 0.0f };
	particle.transform.translate = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
	particle.velocity = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
	particle.color = { distColor(randomEngine), distColor(randomEngine), distColor(randomEngine), 1.0f };
	particle.lifeTime = distTime(randomEngine);
	particle.currentTime = 0.0f;
	return particle;
}

void Particle::Initialize(
	ID3D12Device* device,
	ID3D12DescriptorHeap* srvDescriptorHeap,
	UINT descriptorSize,
	uint32_t& srvIndexCounter,
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* dxcIncludeHandler
) {
	// 乱数生成器の初期化
	randomEngine_ = std::mt19937(seedGenerator_());

	// 1. Particle用 RootSignature & PSO の構築
	CreateRootSignatureAndPSO(device, dxcUtils, dxcCompiler, dxcIncludeHandler);

	// 2. 頂点データ (四角形プリミティブ: 6頂点) の作成
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

	// 4. Instancing用 ParticleForGPU (StructuredBuffer) リソースの作成
	instancingResource_ = CreateBufferResource(device, sizeof(ParticleForGPU) * kNumMaxInstance);
	instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));

	// 5. Instancing用 SRV の作成
	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = kNumMaxInstance;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);

	instancingSrvHandleCPU_ = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSize, srvIndexCounter);
	instancingSrvHandleGPU_ = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSize, srvIndexCounter++);

	device->CreateShaderResourceView(instancingResource_.Get(), &instancingSrvDesc, instancingSrvHandleCPU_);

	// 6. Particle の初期化
	for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
		particles_[index] = MakeNewParticle(randomEngine_);
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

	// ★ 加算ブレンド (AddBlend) の設定
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
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // 両面表示

	// ★ Depthの書き込みを行わない
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Depthの書き込みを行わない

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
	const float kDeltaTime = 1.0f / 60.0f;
	uint32_t numInstance = 0; // 描画すべきインスタンス数

	for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
		// 生存期間を過ぎていたら更新せず描画対象にしない
		if (particles_[index].lifeTime <= particles_[index].currentTime) {
			continue;
		}

		// 速度を位置に反映
		particles_[index].transform.translate.x += particles_[index].velocity.x * kDeltaTime;
		particles_[index].transform.translate.y += particles_[index].velocity.y * kDeltaTime;
		particles_[index].transform.translate.z += particles_[index].velocity.z * kDeltaTime;

		// 経過時間を足す
		particles_[index].currentTime += kDeltaTime;

		// 行列計算
		Matrix4x4 worldMatrix = MakeAffineMatrix(particles_[index].transform.scale, particles_[index].transform.rotate, particles_[index].transform.translate);
		Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);

		// GPU転送バッファにセット
		instancingData_[numInstance].WVP = worldViewProjectionMatrix;
		instancingData_[numInstance].World = worldMatrix;
		instancingData_[numInstance].color = particles_[index].color;

		// ★ α値を徐々に0に近づける
		float alpha = 1.0f - (particles_[index].currentTime / particles_[index].lifeTime);
		instancingData_[numInstance].color.w = alpha;

		// 生きているParticleの数を1つカウントする
		++numInstance;
	}

	numInstance_ = numInstance;
}

void Particle::Draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU) {
	// PSO & RootSignatureのセット
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	// ルートパラメータのバインド
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU_); // Instancing用 StructuredBuffer SRV
	commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);   // テクスチャ SRV

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// 描画 (生きている numInstance_ だけ描画)
	commandList->DrawInstanced(6, numInstance_, 0, 0);
}

void Particle::DrawImGui() {
#ifdef USE_IMGUI
	if (ImGui::Button("Respawn All Particles")) {
		for (uint32_t i = 0; i < kNumMaxInstance; ++i) {
			particles_[i] = MakeNewParticle(randomEngine_);
		}
	}

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

	if (ImGui::CollapsingHeader("Instances Parameter", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Active Particles: %u / %u", numInstance_, kNumMaxInstance);
		for (uint32_t i = 0; i < kNumMaxInstance; ++i) {
			ImGui::PushID(static_cast<int>(i));
			std::string label = std::format("Particle Instance {}", i);

			if (ImGui::TreeNode(label.c_str())) {
				ImGui::ColorEdit4("Color", &particles_[i].color.x);
				ImGui::SliderFloat3("Position", &particles_[i].transform.translate.x, -10.0f, 10.0f);
				ImGui::SliderFloat3("Velocity", &particles_[i].velocity.x, -5.0f, 5.0f);
				ImGui::SliderFloat3("Rotate", &particles_[i].transform.rotate.x, -3.14f, 3.14f);
				ImGui::SliderFloat3("Scale", &particles_[i].transform.scale.x, 0.01f, 5.0f);
				ImGui::Text("LifeTime: %.2f / %.2f s", particles_[i].currentTime, particles_[i].lifeTime);
				if (ImGui::Button("Reset / Respawn")) {
					particles_[i] = MakeNewParticle(randomEngine_);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}
#endif
}