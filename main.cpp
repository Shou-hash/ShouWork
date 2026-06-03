#include "Common.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include <numbers>
#include <vector>
#include <random>

// DXGIファクトリーの実体定義
IDXGIFactory7* dxgiFactory = nullptr;

// 頂点データ構造体の定義
struct VertexData {
	Vector4 position;
	float u, v;
};

// --- Scene 2 演出用の大量の三角形データ構造体 ---
struct ParticleTriangle {
	struct Transform transform;
	struct Transform transform2; // 2枚交差用
	float speed;
	float rotSpeed;
	Vector3 direction;          // 3D的な移動方向用
	float baseScale;            // ★ かっこいい演出用：初期スケールの記憶
	float timeOffset;           // ★ かっこいい演出用：個別の時間軸のズレ
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
		CW_USEDEFAULT,
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

	// Scene 1 用のWVPリソース
	ID3D12Resource* wvpResource = CreateBufferResource(device, sizeof(Matrix4x4));
	Matrix4x4* wvpData = nullptr;
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	*wvpData = MakeIdentity4x4();

	ID3D12Resource* wvpResource2 = CreateBufferResource(device, sizeof(Matrix4x4));
	Matrix4x4* wvpData2 = nullptr;
	wvpResource2->Map(0, nullptr, reinterpret_cast<void**>(&wvpData2));
	*wvpData2 = MakeIdentity4x4();

	// --- Scene 2 用の大量の定数バッファ ---
	const int kMaxParticles = 300;
	std::vector<ID3D12Resource*> particleWVPResources1(kMaxParticles);
	std::vector<ID3D12Resource*> particleWVPResources2(kMaxParticles);
	std::vector<Matrix4x4*> particleWVPData1(kMaxParticles);
	std::vector<Matrix4x4*> particleWVPData2(kMaxParticles);

	// ★ 各パーティクル個別にマテリアル（色・透明度）を制御するためのリソースを追加
	std::vector<ID3D12Resource*> particleMaterialResources(kMaxParticles);
	std::vector<Vector4*> particleMaterialData(kMaxParticles);

	for (int i = 0; i < kMaxParticles; i++) {
		particleWVPResources1[i] = CreateBufferResource(device, sizeof(Matrix4x4));
		particleWVPResources1[i]->Map(0, nullptr, reinterpret_cast<void**>(&particleWVPData1[i]));
		*particleWVPData1[i] = MakeIdentity4x4();

		particleWVPResources2[i] = CreateBufferResource(device, sizeof(Matrix4x4));
		particleWVPResources2[i]->Map(0, nullptr, reinterpret_cast<void**>(&particleWVPData2[i]));
		*particleWVPData2[i] = MakeIdentity4x4();

		particleMaterialResources[i] = CreateBufferResource(device, sizeof(Vector4));
		particleMaterialResources[i]->Map(0, nullptr, reinterpret_cast<void**>(&particleMaterialData[i]));
		*particleMaterialData[i] = Vector4(0.0f, 0.0f, 1.0f, 1.0f);
	}

	// --- 演出用パーティクルデータの初期化 ---
	std::vector<ParticleTriangle> particles(kMaxParticles);
	std::random_device seed_gen;
	std::mt19937 engine(seed_gen());

	std::uniform_real_distribution<float> distRot(0.0f, 6.28f);
	std::uniform_real_distribution<float> distScale(0.1f, 0.35f);
	std::uniform_real_distribution<float> distSpeed(0.03f, 0.08f); // ★ 疾走感を出すために少し速度上限をアップ
	std::uniform_real_distribution<float> distZ(-10.0f, 0.0f);    // ★ 奥行きを深めに変更
	std::uniform_real_distribution<float> distOffset(0.0f, 100.0f);

	for (int i = 0; i < kMaxParticles; i++) {
		float scale = distScale(engine);
		particles[i].baseScale = scale; // ★ 初期サイズを記憶
		particles[i].transform.scale = { scale, scale, scale };
		particles[i].timeOffset = distOffset(engine); // ★ 動きをバラけさせるオフセット

		float theta = distRot(engine);
		float phi = distRot(engine) * 0.5f;

		particles[i].direction.x = sinf(phi) * cosf(theta);
		particles[i].direction.y = sinf(phi) * sinf(theta);
		particles[i].direction.z = cosf(phi) + 0.5f;

		particles[i].transform.translate = {
			particles[i].direction.x * 0.5f,
			particles[i].direction.y * 0.5f,
			distZ(engine)
		};
		particles[i].transform.rotate = { distRot(engine), distRot(engine), distRot(engine) };

		particles[i].transform2 = particles[i].transform;
		particles[i].transform2.rotate.y += 0.5f;

		particles[i].speed = distSpeed(engine);
		particles[i].rotSpeed = distSpeed(engine) * 1.2f;
	}

	// Scene 1 初期位置設定
	struct Transform transform = { {1.0f, 1.0f, 1.0f}, {0.0f, -0.75f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	struct Transform transform2 = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.97f, 0.0f}, {0.0f, 0.36f, 0.0f} };
	struct Transform cameraTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -5.0f} };

	Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
	Matrix4x4 viewMatrix = Inverse(cameraMatrix);
	Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);

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

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3] = {};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

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

	ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * 3);
	ID3D12Resource* vertexResource2 = CreateBufferResource(device, sizeof(VertexData) * 3);

	// --- マテリアル（色指定）リソースの初期化 ---
	ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Vector4));
	Vector4* materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	*materialData = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // デフォルト値を白に変更してテクスチャ本来の色を維持

	// ★ 2つ目の三角形用のマテリアル（色指定）リソースを追加定義
	ID3D12Resource* materialResource2 = CreateBufferResource(device, sizeof(Vector4));
	Vector4* materialData2 = nullptr;
	materialResource2->Map(0, nullptr, reinterpret_cast<void**>(&materialData2));
	*materialData2 = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

#pragma endregion

	DirectX::ScratchImage mipImages = LoadTexture("Resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	ID3D12Resource* textureResource1 = CreateTextureResource(device, metadata);
	UploadTextureData(textureResource1, mipImages);

	DirectX::ScratchImage mipImages2 = LoadTexture("Resources/monsterBall.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	ID3D12Resource* textureResource2 = CreateTextureResource(device, metadata2);
	UploadTextureData(textureResource2, mipImages2);

	// ★ パスを Resources/white1x1.png に修正
	DirectX::ScratchImage mipImages3 = LoadTexture("Resources/white1x1.png");
	const DirectX::TexMetadata& metadata3 = mipImages3.GetMetadata();
	ID3D12Resource* textureResource3 = CreateTextureResource(device, metadata3);
	UploadTextureData(textureResource3, mipImages3);

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = kClientWidth;
	resourceDesc.Height = kClientHeight;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	ID3D12Resource* depthStencilResource = nullptr;
	hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&depthStencilResource)
	);
	assert(SUCCEEDED(hr));

	ID3D12DescriptorHeap* dsvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	device->CreateDepthStencilView(depthStencilResource, &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc1{};
	srvDesc1.Format = metadata.format;
	srvDesc1.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc1.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc1.Texture2D.MipLevels = UINT(metadata.mipLevels);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU1 = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU1 = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	textureSrvHandleCPU1.ptr += descriptorSize * 1;
	textureSrvHandleGPU1.ptr += descriptorSize * 1;
	device->CreateShaderResourceView(textureResource1, &srvDesc1, textureSrvHandleCPU1);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	textureSrvHandleCPU2.ptr += descriptorSize * 2;
	textureSrvHandleGPU2.ptr += descriptorSize * 2;
	device->CreateShaderResourceView(textureResource2, &srvDesc2, textureSrvHandleCPU2);

	// ★ 3番目のテクスチャ用SRVの生成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc3{};
	srvDesc3.Format = metadata3.format;
	srvDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc3.Texture2D.MipLevels = UINT(metadata3.mipLevels);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU3 = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3 = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	textureSrvHandleCPU3.ptr += descriptorSize * 3;
	textureSrvHandleGPU3.ptr += descriptorSize * 3;
	device->CreateShaderResourceView(textureResource3, &srvDesc3, textureSrvHandleCPU3);

#ifdef USE_IMGUI
	// ★ ImGui用のハンドル位置を衝突回避のためずらします
	D3D12_CPU_DESCRIPTOR_HANDLE imguiSrvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	imguiSrvHandleCPU.ptr += descriptorSize * 4;
	D3D12_GPU_DESCRIPTOR_HANDLE imguiSrvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	imguiSrvHandleGPU.ptr += descriptorSize * 4;

	D3D12_CPU_DESCRIPTOR_HANDLE imguiSrvHandleCPU1 = imguiSrvHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE imguiSrvHandleGPU1 = imguiSrvHandleGPU;
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

	// シーン管理用の変数
	int currentScene = 0;
	int previousScene = 0; // ★ 前のフレームのシーンを記録する変数

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

	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	ID3D12RootSignature* rootSignature = nullptr;
	hr = device->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

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

	// ★ かっこいい演出（フェード処理）用：通常描画用の不透明ブレンドステート
	D3D12_BLEND_DESC blendDescOpaque{};
	blendDescOpaque.RenderTarget[0].BlendEnable = FALSE;
	blendDescOpaque.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// ★ かっこいい演出用：きれいに重なり合うための半透明アルファブレンドステート
	D3D12_BLEND_DESC blendDescAlpha{};
	blendDescAlpha.RenderTarget[0].BlendEnable = TRUE;
	blendDescAlpha.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDescAlpha.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDescAlpha.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDescAlpha.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDescAlpha.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDescAlpha.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDescAlpha.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	IDxcBlob* vertexShaderBlob = CompileShader(L"Object3d.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, dxcIncludeHandler);
	assert(vertexShaderBlob != nullptr);

	IDxcBlob* pixelShaderBlob = CompileShader(L"Object3d.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, dxcIncludeHandler);
	assert(pixelShaderBlob != nullptr);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature;
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = blendDescOpaque; // デフォルトは不透明
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 通常用の不透明PSO
	ID3D12PipelineState* graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	// ★ かっこいい演出用：アルファブレンドを有効にしたScene 2専用のPSOを生成
	graphicsPipelineStateDesc.BlendState = blendDescAlpha;
	// 透過オブジェクトがきれいに重なるよう、深度バッファへの書き込みはOFF（読み込みのみ有効）にするのがセオリーです
	graphicsPipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ID3D12PipelineState* alphaBlendPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&alphaBlendPipelineState));
	assert(SUCCEEDED(hr));

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = sizeof(VertexData) * 3;
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView2{};
	vertexBufferView2.BufferLocation = vertexResource2->GetGPUVirtualAddress();
	vertexBufferView2.SizeInBytes = sizeof(VertexData) * 3;
	vertexBufferView2.StrideInBytes = sizeof(VertexData);

	VertexData* vertexData = nullptr;
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	vertexData[0] = { {-0.5f, -0.5f, 0.0f, 1.0f}, 0.0f, 1.0f };
	vertexData[1] = { { 0.0f,  0.5f, 0.0f, 1.0f}, 0.5f, 0.0f };
	vertexData[2] = { { 0.5f, -0.5f, 0.0f, 1.0f}, 1.0f, 1.0f };

	VertexData* vertexData2 = nullptr;
	vertexResource2->Map(0, nullptr, reinterpret_cast<void**>(&vertexData2));
	vertexData2[0] = { {-0.5f, -0.5f, 0.0f, 1.0f}, 0.0f, 1.0f };
	vertexData2[1] = { { 0.0f,  0.5f, 0.0f, 1.0f}, 0.5f, 0.0f };
	vertexData2[2] = { { 0.5f, -0.5f, 0.0f, 1.0f}, 1.0f, 1.0f };

#pragma endregion

	// ★ 切り替え管理変数
	bool useMonsterBall = false;
	int selectedTextureIndex = 0;  // Object 1用のテクスチャインデックス
	int selectedTextureIndex2 = 1; // ★ 追加：Object 2用のテクスチャインデックス（初期値はuvChecker）
	float scene2Color[4] = { 0.05f, 0.05f, 0.1f, 1.0f }; // サイバー感のある少し暗い紺色を初期値に
	float globalTime = 0.0f;                             // ★ アニメーション計算用のグローバルタイマー

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			globalTime += 0.016f; // タイマーを進める（約60fps想定）

#ifdef USE_IMGUI
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			// --- シーンコントローラー ---
			ImGui::Begin("Scene Controller");
			if (ImGui::BeginTabBar("SceneTabs")) {
				if (ImGui::BeginTabItem("Scene 1 (Interactive)")) {
					currentScene = 0;
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Scene 2 (Performance)")) {
					currentScene = 1;
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
			ImGui::End();
#endif

			// ★ シーンが切り替わった瞬間を検知するロジック
			if (currentScene == 1 && previousScene == 0) {
				// Scene 2 に変わったら、テクスチャを強制的に white1x1.png (インデックス 2) に設定
				selectedTextureIndex = 2;
				selectedTextureIndex2 = 2; // ★ Object 2用もScene 2ではリセット
				// 色を純粋な青色 (R=0.0, G=0.0, B=1.0, A=1.0) に上書き
				if (materialData != nullptr) {
					*materialData = Vector4(0.0f, 0.0f, 1.0f, 1.0f);
				}
				if (materialData2 != nullptr) {
					*materialData2 = Vector4(0.0f, 0.0f, 1.0f, 1.0f);
				}
			}
			previousScene = currentScene; // 現在の状態を保存

			useMonsterBall = (selectedTextureIndex == 0);

			if (currentScene == 0) {
#ifdef USE_IMGUI
				// --- Scene 1: 設定ウィンドウ ---
				ImGui::Begin("Settings");

				static int currentModelIndex = 0;
				const char* models[] = { "Triangle" };
				ImGui::SetNextItemWidth(200.0f);
				ImGui::Combo("Model", &currentModelIndex, models, _countof(models));
				ImGui::Button("Create");

				ImGui::Separator();

				// ★ 「Object 1」と「Object 2」を両方IMGUIで操作・色変えできるように変更
				if (ImGui::TreeNodeEx("Objects", ImGuiTreeNodeFlags_DefaultOpen))
				{
					if (ImGui::TreeNodeEx("Object 1", ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::SetNextItemWidth(200.0f);
						ImGui::DragFloat3("Translate", &transform.translate.x, 0.01f, -3.0f, 3.0f, "%.3f");

						ImGui::SetNextItemWidth(200.0f);
						ImGui::DragFloat3("Rotate", &transform.rotate.x, 0.01f, -3.14f, 3.14f, "%.3f");

						ImGui::SetNextItemWidth(200.0f);
						ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f, 0.1f, 3.0f, "%.3f");

						ImGui::SetNextItemWidth(200.0f);
						ImGui::ColorEdit4("color", reinterpret_cast<float*>(materialData), ImGuiColorEditFlags_Float);

						// ★ Object 1 個別のテクスチャ選択
						ImGui::SetNextItemWidth(200.0f);
						if (ImGui::TreeNode("Texture Select"))
						{
							if (ImGui::Selectable("resources/monsterBall.png", selectedTextureIndex == 0)) { selectedTextureIndex = 0; }
							if (ImGui::Selectable("resources/uvChecker.png", selectedTextureIndex == 1)) { selectedTextureIndex = 1; }
							if (ImGui::Selectable("white1x1.png", selectedTextureIndex == 2)) { selectedTextureIndex = 2; }
							ImGui::TreePop();
						}

						ImGui::TreePop();
					}

					if (ImGui::TreeNodeEx("Object 2", ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::SetNextItemWidth(200.0f);
						ImGui::DragFloat3("Translate", &transform2.translate.x, 0.01f, -3.0f, 3.0f, "%.3f");

						ImGui::SetNextItemWidth(200.0f);
						ImGui::DragFloat3("Rotate", &transform2.rotate.x, 0.01f, -3.14f, 3.14f, "%.3f");

						ImGui::SetNextItemWidth(200.0f);
						ImGui::DragFloat3("Scale", &transform2.scale.x, 0.01f, 0.1f, 3.0f, "%.3f");

						ImGui::SetNextItemWidth(200.0f);
						ImGui::ColorEdit4("color", reinterpret_cast<float*>(materialData2), ImGuiColorEditFlags_Float);

						// ★ 追加：Object 2 個別のテクスチャ選択UI
						ImGui::SetNextItemWidth(200.0f);
						if (ImGui::TreeNode("Texture Select##Obj2"))
						{
							if (ImGui::Selectable("resources/monsterBall.png##Obj2", selectedTextureIndex2 == 0)) { selectedTextureIndex2 = 0; }
							if (ImGui::Selectable("resources/uvChecker.png##Obj2", selectedTextureIndex2 == 1)) { selectedTextureIndex2 = 1; }
							if (ImGui::Selectable("white1x1.png##Obj2", selectedTextureIndex2 == 2)) { selectedTextureIndex2 = 2; }
							ImGui::TreePop();
						}

						ImGui::TreePop();
					}

					ImGui::Button("Delete");

					if (ImGui::TreeNodeEx("Material (Common)", ImGuiTreeNodeFlags_DefaultOpen))
					{
						static float uvTranslate[2] = { 0.0f, 0.0f };
						static float uvRotate = 0.0f;
						static float uvScale[2] = { 1.0f, 1.0f };
						static int lightingType = 0;
						const char* lightings[] = { "None", "Lambert", "Half-Lambert" };

						ImGui::SetNextItemWidth(200.0f);
						ImGui::DragFloat2("UVTranslate", uvTranslate, 0.01f, -10.0f, 10.0f, "%.3f");

						ImGui::SetNextItemWidth(200.0f);
						ImGui::DragFloat("UVRotate", &uvRotate, 0.01f, -3.14f, 3.14f, "%.3f");

						ImGui::SetNextItemWidth(200.0f);
						ImGui::DragFloat2("UVScale", uvScale, 0.01f, 0.1f, 10.0f, "%.3f");

						ImGui::SetNextItemWidth(200.0f);
						ImGui::Combo("Lighting", &lightingType, lightings, _countof(lightings));

						ImGui::TreePop();
					}
					ImGui::TreePop();
				}
				if (ImGui::TreeNode("Light")) { ImGui::TreePop(); }
				ImGui::End();
#endif

				Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
				*wvpData = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));

				Matrix4x4 worldMatrix2 = MakeAffineMatrix(transform2.scale, transform2.rotate, transform2.translate);
				*wvpData2 = Multiply(worldMatrix2, Multiply(viewMatrix, projectionMatrix));

			}
			else {
#ifdef USE_IMGUI
				// --- Scene 2: 演出微調整ウィンドウ ---
				ImGui::Begin("Scene 2 Performance Control");
				ImGui::Text("Dynamic Warp Tunnel Effect");

				ImGui::SetNextItemWidth(200.0f);
				ImGui::ColorEdit4("Base Blue Color", reinterpret_cast<float*>(materialData), ImGuiColorEditFlags_Float);

				ImGui::SetNextItemWidth(200.0f);
				ImGui::ColorEdit3("Background Color", scene2Color);
				ImGui::End();
#endif

				// ★ かっこいい演出用の計算ループ
				for (int i = 0; i < kMaxParticles; i++) {
					// 1. 手前方向（Z軸）への高速な移動
					particles[i].transform.translate.z += particles[i].speed * 1.5f;

					// 2. らせん状にうねるような軌道（サイン・コサイン波）を追加してサイバー感を演出
					float waveTime = globalTime * 3.0f + particles[i].timeOffset;
					float warpFactor = (particles[i].transform.translate.z + 10.0f) * 0.15f; // 手前に来るほど広がる

					// 進行方向に、らせんの揺らぎをプラス
					Vector3 finalPos = particles[i].transform.translate;
					finalPos.x += sinf(waveTime) * 0.4f * warpFactor;
					finalPos.y += cosf(waveTime) * 0.4f * warpFactor;

					// 3. スタイリッシュな超高速スピン回転
					particles[i].transform.rotate.x += particles[i].rotSpeed * 1.5f;
					particles[i].transform.rotate.y += particles[i].rotSpeed * 2.0f;
					particles[i].transform.rotate.z += particles[i].rotSpeed * 0.8f;

					// 4. 手前に迫るにつれてスケールが大きくダイナミックに変形する表現
					float currentScale = particles[i].baseScale * (1.0f + warpFactor * 2.0f);
					particles[i].transform.scale = { currentScale, currentScale, currentScale };

					// 5. 画面外（カメラの後ろ）に抜けたら奥にリポーン
					if (particles[i].transform.translate.z > 2.0f) {
						particles[i].transform.translate.z = -10.0f; // 遥か彼方の奥へリセット

						// 散らばる初期位置を再計算
						float theta = distRot(engine);
						particles[i].transform.translate.x = cosf(theta) * 1.2f;
						particles[i].transform.translate.y = sinf(theta) * 1.2f;
					}

					// 2枚交差用にも位置と回転を同期
					particles[i].transform2.translate = finalPos;
					particles[i].transform2.rotate = particles[i].transform.rotate;
					particles[i].transform2.rotate.y += 0.785f; // 45度ずらしてかっこいいクロス形状に

					// 6. アルファフェードの計算（奥と手前で綺麗にフェードアウト）
					float alpha = 1.0f;
					if (particles[i].transform.translate.z < -7.0f) {
						// 湧き出し時のフェードイン
						alpha = (particles[i].transform.translate.z - (-10.0f)) / 3.0f;
					}
					else if (particles[i].transform.translate.z > -1.0f) {
						// 画面手前でのフェードアウト
						alpha = (2.0f - particles[i].transform.translate.z) / 3.0f;
					}
					alpha = std::clamp(alpha, 0.0f, 1.0f);

					// 時間経過で少しサイバーに発光が明滅するようなカラーパルスを青色に掛け合わせ
					float pulse = 0.8f + 0.2f * sinf(globalTime * 5.0f + particles[i].timeOffset);
					if (particleMaterialData[i] != nullptr) {
						particleMaterialData[i]->x = materialData->x * pulse; // R
						particleMaterialData[i]->y = materialData->y * pulse; // G
						particleMaterialData[i]->z = materialData->z * pulse; // B
						particleMaterialData[i]->w = alpha;                    // A (透明度)
					}

					// ワールド行列の生成と転送
					Matrix4x4 matWorld1 = MakeAffineMatrix(particles[i].transform.scale, particles[i].transform.rotate, finalPos);
					*particleWVPData1[i] = Multiply(matWorld1, Multiply(viewMatrix, projectionMatrix));

					// 修正箇所：2つ目の板ポリゴンの回転行列には、元コード通り particles[i].transform2.rotate を使用するよう修正
					Matrix4x4 matWorld2 = MakeAffineMatrix(particles[i].transform.scale, particles[i].transform2.rotate, finalPos);
					*particleWVPData2[i] = Multiply(matWorld2, Multiply(viewMatrix, projectionMatrix));
				}
			}

#ifdef USE_IMGUI
			ImGui::Render();
#endif

			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = swapChainResources[backBufferIndex];
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			commandList->ResourceBarrier(1, &barrier);

			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);

			float clearColor[4];
			if (currentScene == 0) {
				clearColor[0] = 0.1f;
				clearColor[1] = 0.25f;
				clearColor[2] = 0.5f;
				clearColor[3] = 1.0f;
			}
			else {
				clearColor[0] = scene2Color[0];
				clearColor[1] = scene2Color[1];
				clearColor[2] = scene2Color[2];
				clearColor[3] = 1.0f;
			}

			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			D3D12_VIEWPORT viewport{ 0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 1.0f };
			D3D12_RECT scissorRect{ 0, 0, kClientWidth, kClientHeight };
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &scissorRect);

			commandList->SetGraphicsRootSignature(rootSignature);
			ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
			commandList->SetDescriptorHeaps(1, descriptorHeaps);

			// ★ シーンに応じてパイプライン状態（PSO）を動的切り替え
			if (currentScene == 0) {
				commandList->SetPipelineState(graphicsPipelineState); // 通常シーン用（不透明）
			}
			else {
				commandList->SetPipelineState(alphaBlendPipelineState); // 演出シーン用（アルファブレンド＋深度書き込みOFF）
			}

			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// Object 1用の選択されたテクスチャのGPUハンドルを取得
			D3D12_GPU_DESCRIPTOR_HANDLE currentTextureHandle = textureSrvHandleGPU1;
			if (selectedTextureIndex == 0) { currentTextureHandle = textureSrvHandleGPU2; } // monsterBall
			else if (selectedTextureIndex == 1) { currentTextureHandle = textureSrvHandleGPU1; } // uvChecker
			else if (selectedTextureIndex == 2) { currentTextureHandle = textureSrvHandleGPU3; } // white1x1

			// ★ 追加：Object 2用の選択されたテクスチャのGPUハンドルを取得
			D3D12_GPU_DESCRIPTOR_HANDLE currentTextureHandle2 = textureSrvHandleGPU1;
			if (selectedTextureIndex2 == 0) { currentTextureHandle2 = textureSrvHandleGPU2; } // monsterBall
			else if (selectedTextureIndex2 == 1) { currentTextureHandle2 = textureSrvHandleGPU1; } // uvChecker
			else if (selectedTextureIndex2 == 2) { currentTextureHandle2 = textureSrvHandleGPU3; } // white1x1

			if (currentScene == 0) {
				// 1つ目の三角形の描画（ImGuiでの選択によってテクスチャが切り替わる）
				commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(2, currentTextureHandle);
				commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
				commandList->DrawInstanced(3, 1, 0, 0);

				// ★ 2つ目の三角形の描画（固定ではなく、個別に選択された currentTextureHandle2 をバインド）
				commandList->SetGraphicsRootConstantBufferView(0, materialResource2->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, wvpResource2->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(2, currentTextureHandle2); // 変更：動的に選択されたハンドルを適用
				commandList->IASetVertexBuffers(0, 1, &vertexBufferView2);
				commandList->DrawInstanced(3, 1, 0, 0);
			}
			else {
				// ★ Scene 2：パーティクルごとに固有のマテリアル（透明度データ含む）をバインドして描画
				for (int i = 0; i < kMaxParticles; i++) {
					commandList->SetGraphicsRootConstantBufferView(0, particleMaterialResources[i]->GetGPUVirtualAddress());
					commandList->SetGraphicsRootConstantBufferView(1, particleWVPResources1[i]->GetGPUVirtualAddress());
					commandList->SetGraphicsRootDescriptorTable(2, currentTextureHandle);
					commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
					commandList->DrawInstanced(3, 1, 0, 0);

					commandList->SetGraphicsRootConstantBufferView(0, particleMaterialResources[i]->GetGPUVirtualAddress());
					commandList->SetGraphicsRootConstantBufferView(1, particleWVPResources2[i]->GetGPUVirtualAddress());
					commandList->SetGraphicsRootDescriptorTable(2, currentTextureHandle);
					commandList->IASetVertexBuffers(0, 1, &vertexBufferView2);
					commandList->DrawInstanced(3, 1, 0, 0);
				}
			}

#ifdef USE_IMGUI
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif

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

	// 解放処理
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

	if (textureResource1) { textureResource1->Release(); }
	if (textureResource2) { textureResource2->Release(); }
	// 3番目のテクスチャリソースの解放
	if (textureResource3) { textureResource3->Release(); }

#ifdef _DEBUG
	debugController->Release();
#endif 

	CloseWindow(hwnd);

#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif 

	vertexResource->Release();
	vertexResource2->Release();
	wvpResource->Release();
	wvpResource2->Release();

	for (int i = 0; i < kMaxParticles; i++) {
		particleWVPResources1[i]->Release();
		particleWVPResources2[i]->Release();
		particleMaterialResources[i]->Release(); // ★ 追加したマテリアルの解放
	}

	depthStencilResource->Release();
	dsvDescriptorHeap->Release();
	srvDescriptorHeap->Release();
	graphicsPipelineState->Release();
	alphaBlendPipelineState->Release(); // ★ 追加したアルファ用PSOの解放
	signatureBlob->Release();
	if (errorBlob) { errorBlob->Release(); }
	rootSignature->Release();
	pixelShaderBlob->Release();
	vertexShaderBlob->Release();
	materialResource->Release();
	materialResource2->Release(); // ★ 追加したマテリアル2の解放

	CoUninitialize();
	return 0;
}