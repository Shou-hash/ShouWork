
#include "Common.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include <numbers>

		// DXGIファクトリーの実体定義
		IDXGIFactory7 * dxgiFactory = nullptr;

	// 頂点データ構造体の定義をトップレベルに移動
	struct VertexData {
		Vector4 position;
		float u, v;
	};

	int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
	{
		CoInitializeEx(0, COINIT_MULTITHREADED);
		SetUnhandledExceptionFilter(ExportDump);

		OutputDebugStringA("Hello, DirectX!\n");

#pragma region 文字列の出力(stringとwstringの相互変換)

		std::string str0{ "STRING!!!" };
		std::string str1{ std::to_string(10) };

		std::filesystem::create_directories("logs");
		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
		std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };
		std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
		std::string logFilePath = "logs/" + dateString + ".log";
		std::ofstream logFile(logFilePath);

		std::wstring wstringValue = L"テスト文字列";
		Log(logFile, ConvertString(std::format(L"WSTRING: {}\n", wstringValue)));
#pragma endregion

#pragma region ウィンドウの作成

		WNDCLASS wc{};
		wc.lpfnWndProc = WindowProc;
		wc.lpszClassName = L"CG2WindowClass";
		wc.hInstance = hInstance;
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		RegisterClass(&wc);

		const int32_t kClientWidth = 1280;
		const int32_t kClientHeight = 720;
		RECT wrc = { 0, 0, kClientWidth, kClientHeight };
		AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

		HWND hwnd = CreateWindow(
			wc.lpszClassName,
			L"LE2C_12_ショウ_ズーウェン",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, // ← '開設当時_USEDEFAULT' から修正済み
			CW_USEDEFAULT,
			wrc.right - wrc.left,
			wrc.bottom - wrc.top,
			nullptr,
			nullptr,
			wc.hInstance,
			nullptr);

		ShowWindow(hwnd, SW_SHOW);

#pragma endregion

#ifdef _DEBUG

		ID3D12Debug1* debugController = nullptr;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
			debugController->SetEnableGPUBasedValidation(TRUE);
		}

#endif // _DEBUG

#pragma region DirectX 12の初期化

		HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
		assert(SUCCEEDED(hr));

		IDXGIAdapter4* useAdapter = nullptr;
		for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
			DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC3 adapterDesc{};
			hr = useAdapter->GetDesc3(&adapterDesc);
			assert(SUCCEEDED(hr));

			if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE))
			{
				Log(logFile, ConvertString(std::format(L"Use Adapter:{}\n", adapterDesc.Description)));
				break;
			}
			useAdapter = nullptr;
		}
		assert(useAdapter != nullptr);

		ID3D12Device* device = nullptr;
		D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_12_2,
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0
		};
		const char* featureLevelStrings[] = {
			"12.2",
			"12.1",
			"12.0"
		};

		for (size_t i = 0; i < _countof(featureLevels); ++i)
		{
			hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
			if (SUCCEEDED(hr))
			{
				Log(logFile, std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
				break;
			}
		}
		assert(device != nullptr);
		Log(logFile, "Complete create D3D12Device!!!\n");

		ID3D12Resource* wvpResource = CreateBufferResource(device, sizeof(Matrix4x4));
		Matrix4x4* wvpData = nullptr;
		wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
		*wvpData = MakeIdentity4x4();

		struct Transform transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
		struct Transform cameraTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -5.0f} };

		Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
		Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
		Matrix4x4 viewMatrix = Inverse(cameraMatrix);
		Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);

		Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
		*wvpData = worldViewProjectionMatrix;

#pragma endregion

#ifdef _DEBUG

		ID3D12InfoQueue* infoQueue = nullptr;
		if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
		{
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

			D3D12_MESSAGE_ID denyIds[] = {
				D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
			};
			D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
			D3D12_INFO_QUEUE_FILTER filter{};
			filter.DenyList.NumIDs = _countof(denyIds);
			filter.DenyList.pIDList = denyIds;
			filter.DenyList.NumSeverities = _countof(severities);
			filter.DenyList.pSeverityList = severities;

			infoQueue->PushStorageFilter(&filter);
			infoQueue->Release();
		}

#endif // _DEBUG

#pragma region コマンドキューを生成する

		ID3D12CommandQueue* commandQueue = nullptr;
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
		hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
		assert(SUCCEEDED(hr));

		ID3D12CommandAllocator* commandAllocator = nullptr;
		hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
		assert(SUCCEEDED(hr));

		ID3D12GraphicsCommandList* commandList = nullptr;
		hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));
		assert(SUCCEEDED(hr));

		IDXGISwapChain4* swapChain = nullptr;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
		swapChainDesc.Width = kClientWidth;
		swapChainDesc.Height = kClientHeight;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
		assert(SUCCEEDED(hr));

		ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
		rtvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

		ID3D12DescriptorHeap* srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

		ID3D12Resource* swapChainResources[2] = { nullptr };
		hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
		assert(SUCCEEDED(hr));
		hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
		assert(SUCCEEDED(hr));

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
		rtvHandles[0] = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);
		rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);

#pragma endregion

#pragma region RootSignature

		D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
		descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		// DescriptorRangeの構造体をしっかりと初期化
		D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
		descriptorRange[0].BaseShaderRegister = 0; // t0 レジスタ
		descriptorRange[0].NumDescriptors = 1;     // 個数は1つ
		descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV
		descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // 自動オフセット

		// 配列の要素数を 3 に変更！
		D3D12_ROOT_PARAMETER rootParameters[3] = {};

		// RootParameter[0] : マテリアル用（PixelShader）
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[0].Descriptor.ShaderRegister = 0;

		// RootParameter[1] : WVP行列用（VertexShader）
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[1].Descriptor.ShaderRegister = 0;

		// RootParameter[2] : テクスチャ用（DescriptorTable）

		rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
		rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

		descriptionRootSignature.pParameters = rootParameters;
		descriptionRootSignature.NumParameters = _countof(rootParameters); // 自動的に 3 になります

		// サンプラー（Sampler）の設定も追加
		D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
		staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
		staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // リピート
		staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
		staticSamplers[0].ShaderRegister = 0; // s0 レジスタ
		staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		descriptionRootSignature.pStaticSamplers = staticSamplers;
		descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

		// サイズ（sizeof(VertexData) * 3）で確保
		ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * 6);

		ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Vector4));
		Vector4* materialData = nullptr;
		materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
		*materialData = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // テクスチャの色をそのまま出すために白(1,1,1,1)を推奨

		// Sprite用の頂点バッファも同様に作成（サイズは6枚分）
		ID3D12Resource* vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 6);

		// 頂点データの設定（例として、正方形を描くための6枚分の頂点データ）
		D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
		vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress(); // ★Sprite用に修正
		vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
		vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

#pragma endregion

		DirectX::ScratchImage mipImages = LoadTexture("Resources/uvChecker.png");
		const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
		ID3D12Resource* textureResource1 = CreateTextureResource(device, metadata);
		UploadTextureData(textureResource1, mipImages);

		// 二枚目のテクスチャ読み込みとリソース生成（元コードの変数名ルールに準拠）
		DirectX::ScratchImage mipImages2 = LoadTexture("Resources/monsterBall.png");
		const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
		ID3D12Resource* textureResource2 = CreateTextureResource(device, metadata2);
		UploadTextureData(textureResource2, mipImages2);

		// Resourceの設定を行う
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Width = kClientWidth; // テクスチャの幅（画面サイズ）
		resourceDesc.Height = kClientHeight; // テクスチャの高さ（画面サイズ）
		resourceDesc.MipLevels = 1; // mipmapの数
		resourceDesc.DepthOrArraySize = 1; // 奥行き or 配列Textureの配列数
		resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
		resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定。
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

		// 利用するHeapの設定
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

		// 深度値のクリア設定（フレームの最初に最も遠い1.0f、ステンシル0でクリアする用）
		D3D12_CLEAR_VALUE depthClearValue{};
		depthClearValue.DepthStencil.Depth = 1.0f;
		depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

		// 設定した内容でリソースを生成する
		ID3D12Resource* depthStencilResource = nullptr;
		hr = device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度書き込み状態
			&depthClearValue, // クリア値を指定
			IID_PPV_ARGS(&depthStencilResource)
		);
		assert(SUCCEEDED(hr));

		// DSVのDescriptorHeapを生成
		ID3D12DescriptorHeap* dsvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

		// DSVの作成
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

		// DepthStencilViewを生成して、DescriptorHeapの先頭に配置
		device->CreateDepthStencilView(depthStencilResource, &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

		// テクスチャ用SRVの作成
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc1{};
		srvDesc1.Format = metadata.format;
		srvDesc1.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc1.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc1.Texture2D.MipLevels = UINT(metadata.mipLevels);

		UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		// 1枚目は先頭から1つ進めた位置（1番目）
		D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU1 = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU1 = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		textureSrvHandleCPU1.ptr += descriptorSize * 1;
		textureSrvHandleGPU1.ptr += descriptorSize * 1;
		device->CreateShaderResourceView(textureResource1, &srvDesc1, textureSrvHandleCPU1);

		// --- 2枚目のテクスチャ用SRVの作成 ---
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
		srvDesc2.Format = metadata2.format;
		srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

		// 2枚目は先頭から2つ進めた位置（2番目）に配置して重複を避ける
		D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		textureSrvHandleCPU2.ptr += descriptorSize * 2;
		textureSrvHandleGPU2.ptr += descriptorSize * 2;
		device->CreateShaderResourceView(textureResource2, &srvDesc2, textureSrvHandleCPU2);


#ifdef USE_IMGUI

		// ImGui用はさらにその後ろ（3番目）を使用するように修正
		D3D12_CPU_DESCRIPTOR_HANDLE imguiSrvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		imguiSrvHandleCPU.ptr += descriptorSize * 3;
		D3D12_GPU_DESCRIPTOR_HANDLE imguiSrvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		imguiSrvHandleGPU.ptr += descriptorSize * 3;

		// ※元コードで imguiSrvHandleCPU1, 2 と分かれていましたが、
		// ImGui自体に渡すハンドルは1つで良いため、変数名が競合する場合は上記のように整理するか、
		// 以下のように元コードの変数名に合わせるなら 3番目、4番目 と進めてください。
		D3D12_CPU_DESCRIPTOR_HANDLE imguiSrvHandleCPU1 = imguiSrvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE imguiSrvHandleGPU1 = imguiSrvHandleGPU;

		D3D12_CPU_DESCRIPTOR_HANDLE imguiSrvHandleCPU2 = imguiSrvHandleCPU;
		imguiSrvHandleCPU2.ptr += descriptorSize;
		D3D12_GPU_DESCRIPTOR_HANDLE imguiSrvHandleGPU2 = imguiSrvHandleGPU;
		imguiSrvHandleGPU2.ptr += descriptorSize;

#endif

#pragma region Imguiの初期化

#ifdef USE_IMGUI

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX12_Init(device,
			swapChainDesc.BufferCount,
			rtvDesc.Format,
			srvDescriptorHeap,
			srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->Build();

#endif // USE_IMGUI

#pragma endregion

		MSG msg{};
		ID3D12Fence* fence = nullptr;
		uint64_t fenceValue = 0;
		hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		assert(SUCCEEDED(hr));

		HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		assert(fenceEvent != nullptr);

		IDxcUtils* dxcUtils = nullptr;
		IDxcCompiler3* dxcCompiler = nullptr;
		hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
		assert(SUCCEEDED(hr));
		hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
		assert(SUCCEEDED(hr));

		IDxcIncludeHandler* dxcIncludeHandler = nullptr;
		hr = dxcUtils->CreateDefaultIncludeHandler(&dxcIncludeHandler);
		assert(SUCCEEDED(hr));

#pragma region PSO

		// シリアライズしてバイナリにする
		ID3DBlob* signatureBlob = nullptr;
		ID3DBlob* errorBlob = nullptr;
		hr = D3D12SerializeRootSignature(&descriptionRootSignature,
			D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
		if (FAILED(hr)) {
			Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
			assert(false);
		}

		// バイナリを元にルートシグネチャを生成
		ID3D12RootSignature* rootSignature = nullptr;
		hr = device->CreateRootSignature(0,
			signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&rootSignature));
		assert(SUCCEEDED(hr));

		// 頂点レイアウトの設定
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};

		inputElementDescs[0].SemanticName = "POSITION";
		inputElementDescs[0].SemanticIndex = 0;
		inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

		inputElementDescs[1].SemanticName = "TEXCOORD";
		inputElementDescs[1].SemanticIndex = 0;
		inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
		inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

		D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
		inputLayoutDesc.pInputElementDescs = inputElementDescs;
		inputLayoutDesc.NumElements = _countof(inputElementDescs);

		// BlendStateの設定
		D3D12_BLEND_DESC blendDesc{};
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		// RasterizerStateの設定
		D3D12_RASTERIZER_DESC rasterizerDesc{};
		rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

		// 各種Shaderをコンパイル
		IDxcBlob* vertexShaderBlob = CompileShader(L"Object3d.VS.hlsl",
			L"vs_6_0", dxcUtils, dxcCompiler, dxcIncludeHandler);
		assert(vertexShaderBlob != nullptr);

		IDxcBlob* pixelShaderBlob = CompileShader(L"Object3d.PS.hlsl",
			L"ps_6_0", dxcUtils, dxcCompiler, dxcIncludeHandler);
		assert(pixelShaderBlob != nullptr);

		// グラフィックスパイプラインステートの設定
		D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
		graphicsPipelineStateDesc.pRootSignature = rootSignature;
		graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
		graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
		graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
		graphicsPipelineStateDesc.BlendState = blendDesc;
		graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

		graphicsPipelineStateDesc.NumRenderTargets = 1;
		graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		graphicsPipelineStateDesc.SampleDesc.Count = 1;
		graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

		// depthStencilDescの設定
		D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
		//　Depthの機能を有効化する
		depthStencilDesc.DepthEnable = true;
		// 書き込みします
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		// 比較関数はLessにする（小さい値ほど手前）
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

		// DepthStencilStateをグラフィックスパイプラインステートの設定に反映させる
		graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
		// DepthStencilViewのFormatをグラフィックスパイプラインステートの設定に反映させる
		graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		// パイプラインステートの生成
		ID3D12PipelineState* graphicsPipelineState = nullptr;
		hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
		assert(SUCCEEDED(hr));

		// 頂点バッファビューを作成する
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
		vertexBufferView.SizeInBytes = sizeof(VertexData) * 6;
		vertexBufferView.StrideInBytes = sizeof(VertexData);

		// 頂点リソースにデータを書き込む
		VertexData* vertexData = nullptr;
		vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

#pragma endregion

#pragma region 描画数値

		// Sprite用の頂点データも同様に設定する
		VertexData* vertexDataSprite = nullptr;
		vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

		vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f };
		vertexDataSprite[0].u = 0.0f;
		vertexDataSprite[0].v = 1.0f;
		vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
		vertexDataSprite[1].u = 0.0f;
		vertexDataSprite[1].v = 0.0f;
		vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };
		vertexDataSprite[2].u = 1.0f;
		vertexDataSprite[2].v = 1.0f;

		vertexDataSprite[3].position = { 0.0f, 0.0f, 0.0f, 1.0f };
		vertexDataSprite[3].u = 0.0f;
		vertexDataSprite[3].v = 0.0f;
		vertexDataSprite[4].position = { 640.0f, 0.0f, 0.0f, 1.0f };
		vertexDataSprite[4].u = 1.0f;
		vertexDataSprite[4].v = 0.0f;
		vertexDataSprite[5].position = { 640.0f, 360.0f, 0.0f, 1.0f };
		vertexDataSprite[5].u = 1.0f;
		vertexDataSprite[5].v = 1.0f;

		ID3D12Resource* transformationMatrixResourceSprite = CreateBufferResource(device, sizeof(Matrix4x4));

		Matrix4x4* transformationMatrixDataSprite = nullptr;

		transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));

		*transformationMatrixDataSprite = MakeIdentity4x4();

		struct Transform transformSprite { { 1.0f, 1.0f, 1.0f }, { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,0.0f } };

		Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);

		Matrix4x4 vireMatrixSprite = MakeIdentity4x4();

		static float orthoLeft = 0.0f;
		static float orthoRight = 1280.0f;
		static float orthoTop = 0.0f;
		static float orthoBottom = 720.0f;

		Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(orthoLeft, orthoRight, orthoTop, orthoBottom, 0.0f, 100.0f);

		Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(vireMatrixSprite, projectionMatrixSprite));

		*transformationMatrixDataSprite = worldViewProjectionMatrixSprite;

		// 球の頂点数を計算するための分割数
		const uint32_t kSubdivision = 16;

		const uint32_t kNumSphereVertices = (kSubdivision) * (kSubdivision) * 6;

		Sphere sphere = { { 0.0f, 0.0f, 0.0f }, 1.0f };

		// 球（Sphere）用のリソース作成
		ID3D12Resource* vertexResourceSphere3D = CreateBufferResource(device, sizeof(VertexData) * kNumSphereVertices);
		D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere3D{};
		vertexBufferViewSphere3D.BufferLocation = vertexResourceSphere3D->GetGPUVirtualAddress();
		vertexBufferViewSphere3D.SizeInBytes = sizeof(VertexData) * kNumSphereVertices;
		vertexBufferViewSphere3D.StrideInBytes = sizeof(VertexData);

		VertexData* vertexDataSphere3D = nullptr;
		vertexResourceSphere3D->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSphere3D));

		// 球用の行列データリソース
		ID3D12Resource* transformationMatrixResourceSphere3D = CreateBufferResource(device, sizeof(Matrix4x4));
		Matrix4x4* transformationMatrixDataSphere3D = nullptr;
		transformationMatrixResourceSphere3D->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSphere3D));

		// 数理的な球の頂点生成アルゴリズム
		float latStep = std::numbers::pi_v<float> / float(kSubdivision);       // 緯度分割のステップ
		float lonStep = 2.0f * std::numbers::pi_v<float> / float(kSubdivision); // 経度分割のステップ

		uint32_t sphereVertexIndex = 0;

		for (uint32_t lat = 0; lat < kSubdivision; ++lat) {
			float lat0 = -std::numbers::pi_v<float> / 2.0f + float(lat) * latStep;
			float lat1 = lat0 + latStep;

			for (uint32_t lon = 0; lon < kSubdivision; ++lon) {
				float lon0 = float(lon) * lonStep;
				float lon1 = lon0 + lonStep;

				// 四角形の4つの頂点のローカル座標を計算
				// 頂点A (lat0, lon0)
				Vector4 pA = { cosf(lat0) * cosf(lon0), sinf(lat0), cosf(lat0) * sinf(lon0), 1.0f };
				// 頂点B (lat1, lon0)
				Vector4 pB = { cosf(lat1) * cosf(lon0), sinf(lat1), cosf(lat1) * sinf(lon0), 1.0f };
				// 頂点C (lat0, lon1)
				Vector4 pC = { cosf(lat0) * cosf(lon1), sinf(lat0), cosf(lat0) * sinf(lon1), 1.0f };
				// 頂点D (lat1, lon1)
				Vector4 pD = { cosf(lat1) * cosf(lon1), sinf(lat1), cosf(lat1) * sinf(lon1), 1.0f };

				// UV座標 (簡易的に割り当て。資料のテクスチャマッピングに準拠)
				float u0 = float(lon) / float(kSubdivision);
				float u1 = float(lon + 1) / float(kSubdivision);
				float v0 = 1.0f - float(lat) / float(kSubdivision);
				float v1 = 1.0f - float(lat + 1) / float(kSubdivision);

				// 三角形1枚目 (A -> B -> C)
				vertexDataSphere3D[sphereVertexIndex++] = { pA, u0, v0 };
				vertexDataSphere3D[sphereVertexIndex++] = { pB, u0, v1 };
				vertexDataSphere3D[sphereVertexIndex++] = { pC, u1, v0 };

				// 三角形2枚目 (C -> B -> D)
				vertexDataSphere3D[sphereVertexIndex++] = { pC, u1, v0 };
				vertexDataSphere3D[sphereVertexIndex++] = { pB, u0, v1 };
				vertexDataSphere3D[sphereVertexIndex++] = { pD, u1, v1 };
			}
		}

		// 三角形の頂点データを設定する
		// 左下 (UV: 0.0, 1.0)
		vertexData[0].position = { -0.5f, -0.5f, 0.0f, 1.0f };
		vertexData[0].u = 0.0f; vertexData[0].v = 1.0f;

		// 上 (UV: 0.5, 0.0)
		vertexData[1].position = { 0.0f, 0.5f, 0.0f, 1.0f };
		vertexData[1].u = 0.5f; vertexData[1].v = 0.0f;

		// 右下 (UV: 1.0, 1.0)
		vertexData[2].position = { 0.5f, -0.5f, 0.0f, 1.0f };
		vertexData[2].u = 1.0f; vertexData[2].v = 1.0f;

		// 右下 (UV: 1.0, 1.0)
		vertexData[3].position = { -0.5f, -0.5f, 0.5f, 1.0f };
		vertexData[3].u = 0.0f; vertexData[3].v = 1.0f;

		// 上 (UV: 0.5, 0.0)
		vertexData[4].position = { 0.0f, 0.0f, 0.0f, 1.0f };
		vertexData[4].u = 0.5f; vertexData[4].v = 0.0f;

		// 左上 (UV: 0.0, 0.0)
		vertexData[5].position = { 0.5f, -0.5f, -0.5f, 1.0f };
		vertexData[5].u = 1.0f; vertexData[5].v = 1.0f;

#pragma endregion

		// ★追加：テクスチャ切り替えフラグ
		bool useMonsterBall = true;
		// ★追加：スプライト用のテクスチャ切り替えフラグを定義
		bool useSpriteMonsterBall = false;

		while (msg.message != WM_QUIT)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				ImGui_ImplDX12_NewFrame();
				ImGui_ImplWin32_NewFrame();
				ImGui::NewFrame();

				// 三角形の回転
				transform.rotate.y += 0.03f;

				// ★追加：球体も同じ速度でY軸回転させる
				sphere.rotate.y += 0.03f;

				Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
				Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
				*wvpData = worldViewProjectionMatrix;

				// 球体用のWVP行列更新（★sphereRotate を適用）
				Matrix4x4 worldMatrixSphere = MakeAffineMatrix({ sphere.radius, sphere.radius, sphere.radius }, sphere.rotate, sphere.center);
				Matrix4x4 worldViewProjectionMatrixSphere = Multiply(worldMatrixSphere, Multiply(viewMatrix, projectionMatrix));
				*transformationMatrixDataSphere3D = worldViewProjectionMatrixSphere;

				ImGui::Begin("Ortho Matrix Config");
				ImGui::SliderFloat("Left", &orthoLeft, -200.0f, 200.0f);
				ImGui::SliderFloat("Right", &orthoRight, 1000.0f, 2000.0f);
				ImGui::SliderFloat("Top", &orthoTop, -200.0f, 200.0f);
				ImGui::SliderFloat("Bottom", &orthoBottom, 500.0f, 1000.0f);
				ImGui::End();

				// 毎フレーム更新して行列に適用
				projectionMatrixSprite = MakeOrthographicMatrix(orthoLeft, orthoRight, orthoTop, orthoBottom, 0.0f, 100.0f);

				ImGui::ShowDemoWindow();

				// ★追加：ImGuiにテクスチャ切り替え用チェックボックスを追加
				ImGui::Checkbox("use MonsterBall Texture", &useMonsterBall);
				// ★追加：IMGUIにスプライト用チェックボックスを追加
				ImGui::Checkbox("use Sprite MonsterBall Texture", &useSpriteMonsterBall);

				ImGui::Render();

				UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition.pResource = swapChainResources[backBufferIndex];
				barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
				barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
				commandList->ResourceBarrier(1, &barrier);
				// 描画先のRTVとDSVを設定する
				D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
				commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle); // 第4引数に&dsvHandleを指定

				// レンダーターゲットのクリア
				float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
				commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

				// 指定した深度で画面全体をクリアする（一番奥の1.0fでクリア）
				commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

				D3D12_VIEWPORT viewport{};
				viewport.Width = kClientWidth;
				viewport.Height = kClientHeight;
				viewport.TopLeftX = 0;
				viewport.TopLeftY = 0;
				viewport.MinDepth = 0.0f;
				viewport.MaxDepth = 1.0f;

				D3D12_RECT scissorRect{};
				scissorRect.left = 0;
				scissorRect.right = kClientWidth;
				scissorRect.top = 0;
				scissorRect.bottom = kClientHeight;

				commandList->RSSetViewports(1, &viewport);
				commandList->RSSetScissorRects(1, &scissorRect);

				commandList->SetGraphicsRootSignature(rootSignature);

				// テクスチャ(SRV)をバインドする前に、必ずDescriptorHeapをセットする！
				ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
				commandList->SetDescriptorHeaps(1, descriptorHeaps);
				commandList->SetPipelineState(graphicsPipelineState);
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				// ★切り替え用のカレントハンドルを特定
				D3D12_GPU_DESCRIPTOR_HANDLE currentTextureHandle = useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU1;
				// ★追加：スプライト用のテクスチャハンドル選択ロジックを追加
				D3D12_GPU_DESCRIPTOR_HANDLE currentSpriteTextureHandle = useSpriteMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU1;

				// --- 1. 3Dオブジェクト（三角形）の描画 ---
				commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(2, currentTextureHandle); // ★カレントハンドルへ変更
				commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
				commandList->DrawInstanced(6, 1, 0, 0);

				// --- 2. 3Dオブジェクト（球体：Sphere）の描画 ---
				commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSphere3D->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(2, currentTextureHandle); // ★カレントハンドルへ変更
				commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSphere3D);
				commandList->DrawInstanced(kNumSphereVertices, 1, 0, 0);

				// --- 3. 2Dオブジェクト（スプライト）の描画 ---
				// 指摘対応：固定ハンドルから選択ハンドルへ変更
				commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
				// 修正箇所：固定の textureSrvHandleGPU1 から、動的に選択される currentSpriteTextureHandle へ変更
				commandList->SetGraphicsRootDescriptorTable(2, currentSpriteTextureHandle); // ★変更：選択されたハンドルを渡すように修正
				commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
				commandList->DrawInstanced(6, 1, 0, 0);

				// =================================================================

				// ImGuiの描画（一番最前面に映すため最後に行う）
				ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

				barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
				barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
				commandList->ResourceBarrier(1, &barrier);

				hr = commandList->Close();
				assert(SUCCEEDED(hr));

				ID3D12CommandList* commandLists[] = { commandList };
				commandQueue->ExecuteCommandLists(1, commandLists);

				swapChain->Present(1, 0);

				fenceValue++;
				commandQueue->Signal(fence, fenceValue);

				if (fence->GetCompletedValue() < fenceValue)
				{
					fence->SetEventOnCompletion(fenceValue, fenceEvent);
					WaitForSingleObject(fenceEvent, INFINITE);
				}

				hr = commandAllocator->Reset();
				assert(SUCCEEDED(hr));
				hr = commandList->Reset(commandAllocator, nullptr);
				assert(SUCCEEDED(hr));
			}
		}

		// (以下、解放処理などは変更ありませんので省略、そのままビルド可能です)
		IDXGIDebug* debug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
		{
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_DETAIL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_DETAIL);
			debug->Release();
		}

		CloseHandle(fenceEvent);
		fence->Release();
		rtvDescriptorHeap->Release();
		swapChainResources[0]->Release();
		swapChainResources[1]->Release();
		swapChain->Release();
		commandList->Release();
		commandAllocator->Release();
		commandQueue->Release();
		device->Release();
		useAdapter->Release();
		dxgiFactory->Release();

		if (textureResource1) {
			textureResource1->Release();
		}
		if (textureResource2) {
			textureResource2->Release();
		}

#ifdef _DEBUG

		debugController->Release();

#endif // _DEBUG

		CloseWindow(hwnd);

#ifdef USE_IMGUI

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

#endif // USE_IMGUI

		vertexResource->Release();
		vertexResourceSprite->Release();
		transformationMatrixResourceSprite->Release();
		vertexResourceSphere3D->Release();
		transformationMatrixResourceSphere3D->Release();
		graphicsPipelineState->Release();
		signatureBlob->Release();
		if (errorBlob)
		{
			errorBlob->Release();
		}
		rootSignature->Release();
		pixelShaderBlob->Release();
		vertexShaderBlob->Release();
		materialResource->Release();

		CoUninitialize();
		return 0;
	}