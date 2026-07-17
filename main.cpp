#include "Common.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include <numbers>
#include "ResourceObject.h"
#include <wrl.h>
#include "Sound.h"
#include "DebugCamera.h"
#pragma comment(lib, "dxgi.lib")

// DXGIファクトリーの実体定義
IDXGIFactory7* dxgiFactory = nullptr;
Microsoft::WRL::ComPtr<ID3D12Device> device;

BYTE key[256] = {};     // 現在のフレームのキー状態
BYTE keyPre[256] = {};  // 1フレーム前のキー状態

// 追加：トリガー処理（キー入力判定関数群）

// キーを押した状態か
bool IsPushKey(uint8_t keyNumber, const BYTE* key) {
	if (key[keyNumber]) {
		return true;
	}
	return false;
}

// キーを離した状態か
bool IsReleaseKey(uint8_t keyNumber, const BYTE* key) {
	if (!key[keyNumber]) {
		return true;
	}
	return false;
}

// キーを押した瞬間か
bool IsTriggerKey(uint8_t keyNumber, const BYTE* key, const BYTE* keyPre) {
	if (key[keyNumber] && !keyPre[keyNumber]) {
		return true;
	}
	return false;
}

// キーを離した瞬間か
bool IsExitKey(uint8_t keyNumber, const BYTE* key, const BYTE* keyPre) {
	if (!key[keyNumber] && keyPre[keyNumber]) {
		return true;
	}
	return false;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
	CoInitializeEx(0, COINIT_MULTITHREADED);
	SetUnhandledExceptionFilter(ExportDump);

	OutputDebugStringA("Hello, DirectX!\n");

	// サウンドシステムの初期化
	Sound* soundManager = Sound::GetInstance();
	soundManager->Initialize();

	// 音声ファイルの読み込み
	// ※ 実行環境に合わせて、"Resources/Alarm.wav" などの実在するパスに書き換えてください
	Sound::SoundData soundData = soundManager->SoundLoadWave("Resources/fanfare.wav");

	// 音声の再生 (テスト再生)
	soundManager->SoundPlayWave(soundData);

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

	ID3D12Resource* wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix* wvpData = nullptr;
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	wvpData->WVP = MakeIdentity4x4();
	wvpData->World = MakeIdentity4x4();

	struct Transform transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	// デバッグカメラの生成と初期化
	DebugCamera debugCamera;
	debugCamera.Initialize();

	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 viewMatrix = debugCamera.GetViewMatrix();
	Matrix4x4 projectionMatrix = debugCamera.GetProjectionMatrix();

	Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	wvpData->WVP = worldViewProjectionMatrix;
	wvpData->World = worldMatrix;

#pragma endregion

	// 追加：DirectInputの初期化
	// DirectInputの初期化
	ResourceObject<IDirectInput8> directInput;
	hr = DirectInput8Create(
		hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
		(void**)directInput.GetAddressOf(), nullptr); // GetAddressOf() で初期化
	assert(SUCCEEDED(hr));

	// キーボードデバイスをResourceObjectで管理
	ResourceObject<IDirectInputDevice8> keyboard;
	hr = directInput->CreateDevice(GUID_SysKeyboard, keyboard.GetAddressOf(), NULL);
	assert(SUCCEEDED(hr));

	// 入力データ形式のセット (operator-> を経由してメソッドを呼び出す)
	hr = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(hr));

	// 排他制御レベルのセット
	hr = keyboard->SetCooperativeLevel(
		hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));

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

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap =
		CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

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

	// マテリアルデータのセットアップ（スライド2枚目「初期化処理の追加」に準拠）
	ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Material));
	Material* materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = 1;
	materialData->uvTransform = MakeIdentity4x4(); // 単位行列で初期化

	ID3D12Resource* directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
	DirectionalLight* directionalLightData = nullptr;
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;

#pragma endregion

#pragma region モデルデータの読み込み

	std::string modelDir = "Resources";
	std::string modelFile = "plane.obj";
	std::string fullPath = modelDir + "/" + modelFile;

	if (!std::filesystem::exists(fullPath))
	{
		MessageBoxA(nullptr, "モデルファイルが見つかりません。", "Resource Error", MB_OK | MB_ICONERROR);
	}

	// 1. 先にモデルを読み込む
	ModelData modelData = LoadObjFile(modelDir, modelFile);

#pragma endregion

#pragma region Textureの読み込みとSRVの作成

	// 2. モデルデータから取得したテクスチャパスを使ってロードする
	// ※ 指摘事項: ハードコード(Resources/uvChecker.png)を廃止し、modelDataのパスを使用
	std::string texturePath = modelDir + "/" + modelData.material.textureFilePath;
	DirectX::ScratchImage mipImages = LoadTexture(texturePath);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource1 = CreateTextureResource(device, metadata);
	UploadTextureData(textureResource1.Get(), mipImages);

	// 3. デバッグ用の2枚目のテクスチャ（モンスターボール）をロードする
	DirectX::ScratchImage mipImages2 = LoadTexture("Resources/monsterBall.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();

	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 = CreateTextureResource(device, metadata2);
	UploadTextureData(textureResource2.Get(), mipImages2);

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

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc1{};
	srvDesc1.Format = metadata.format;
	srvDesc1.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc1.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc1.Texture2D.MipLevels = UINT(metadata.mipLevels);

	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// modelDataから生成したテクスチャ(textureResource1)を登録
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU1 = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU1 = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	textureSrvHandleCPU1.ptr += descriptorSize * 1;
	textureSrvHandleGPU1.ptr += descriptorSize * 1;
	device->CreateShaderResourceView(textureResource1.Get(), &srvDesc1, textureSrvHandleCPU1);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	textureSrvHandleCPU2.ptr += descriptorSize * 2;
	textureSrvHandleGPU2.ptr += descriptorSize * 2;
	device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

#ifdef USE_IMGUI

	D3D12_CPU_DESCRIPTOR_HANDLE imguiSrvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	imguiSrvHandleCPU.ptr += descriptorSize * 3;
	D3D12_GPU_DESCRIPTOR_HANDLE imguiSrvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	imguiSrvHandleGPU.ptr += descriptorSize * 3;

	D3D12_CPU_DESCRIPTOR_HANDLE imguiSrvHandleCPU1 = imguiSrvHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE imguiSrvHandleGPU1 = imguiSrvHandleGPU;

	D3D12_CPU_DESCRIPTOR_HANDLE imguiSrvHandleCPU2 = imguiSrvHandleCPU;
	imguiSrvHandleCPU2.ptr += descriptorSize;
	D3D12_GPU_DESCRIPTOR_HANDLE imguiSrvHandleGPU2 = imguiSrvHandleGPU;
	imguiSrvHandleGPU2.ptr += descriptorSize;

#endif

#pragma endregion

	ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * modelData.vertices.size());

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * modelData.vertices.size());
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	VertexData* vertexData = nullptr;
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

#pragma region Imguiの初期化

#ifdef USE_IMGUI

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(device,
		swapChainDesc.BufferCount,
		rtvDesc.Format,
		srvDescriptorHeap.Get(),
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Build();

#endif

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

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};

	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	IDxcBlob* vertexShaderBlob = CompileShader(L"Object3d.VS.hlsl",
		L"vs_6_0", dxcUtils, dxcCompiler, dxcIncludeHandler);
	assert(vertexShaderBlob != nullptr);

	IDxcBlob* pixelShaderBlob = CompileShader(L"Object3d.PS.hlsl",
		L"ps_6_0", dxcUtils, dxcCompiler, dxcIncludeHandler);
	assert(pixelShaderBlob != nullptr);

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

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	ID3D12PipelineState* graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

#pragma endregion

#pragma region 描画数値

#pragma endregion

	bool useMonsterBall = true;
	bool useSpriteMonsterBall = false;

	float uiLightDirection[3] = { 0.0f, -1.0f, 0.0f };

	float sphereRotate[3] = { 0.0f, 0.0f, 0.0f };

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{

			// 追加：更新処理 (毎フレーム行う)
			// キーボード情報の取得開始
			std::memcpy(keyPre, key, sizeof(key));

			// キーボード情報の取得開始
			keyboard->Acquire();

			// 全キーの入力状態を取得する
			keyboard->GetDeviceState(sizeof(key), key);

			// ==========================================
			// 使い方サンプル
			// ==========================================
			// スペースキー（DIK_SPACE）を押した「瞬間」だけ処理を通す
			if (IsTriggerKey(DIK_SPACE, key, keyPre))
			{
				OutputDebugStringA("Space Triggered!\n");
			}

			// 0キー（DIK_0）を押している「間」ずっと処理を通す
			if (IsPushKey(DIK_0, key))
			{
				OutputDebugStringA("Hit 0\n");
			}

			// ==========================================

			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			ImGui::Checkbox("use MonsterBall Texture", &useMonsterBall);
			ImGui::Checkbox("use Sprite MonsterBall Texture", &useSpriteMonsterBall);

			ImGui::Begin("Lighting Settings");
			ImGui::ColorEdit4("Material Color", &materialData->color.x);
			ImGui::Checkbox("Enable Lighting", reinterpret_cast<bool*>(&materialData->enableLighting));
			ImGui::Separator();
			ImGui::ColorEdit4("Light Color", &directionalLightData->color.x);

			ImGui::SliderFloat3("Light Direction", uiLightDirection, -1.0f, 1.0f);
			ImGui::SliderFloat("Light Intensity", &directionalLightData->intensity, 0.0f, 5.0f);

			ImGui::Separator();
			ImGui::Text("Model Transform");
			// 0〜360度で動かせるスライダー (SphereRotateX, Y, Z)
			ImGui::SliderFloat3("Sphere Rotate", sphereRotate, 0.0f, 360.0f);

			// スライダーの度数法（Degree）をラジアン（Radian）に変換して構造体に適用
			transform.rotate.x = sphereRotate[0] * (std::numbers::pi_v<float> / 180.0f);
			transform.rotate.y = sphereRotate[1] * (std::numbers::pi_v<float> / 180.0f);
			transform.rotate.z = sphereRotate[2] * (std::numbers::pi_v<float> / 180.0f);

			// デバッグカメラの更新
			debugCamera.Update();
			viewMatrix = debugCamera.GetViewMatrix();
			projectionMatrix = debugCamera.GetProjectionMatrix();

			// 行列の再計算と定数バッファへの転送
			worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
			Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
			wvpData->WVP = worldViewProjectionMatrix;
			wvpData->World = worldMatrix;

			float length = std::sqrt(uiLightDirection[0] * uiLightDirection[0] +
				uiLightDirection[1] * uiLightDirection[1] +
				uiLightDirection[2] * uiLightDirection[2]);
			if (length > 0.0f)
			{
				directionalLightData->direction.x = uiLightDirection[0] / length;
				directionalLightData->direction.y = uiLightDirection[1] / length;
				directionalLightData->direction.z = uiLightDirection[2] / length;
			}
			else
			{
				directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
			}

			ImGui::End();

			ImGui::Render();

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

			float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
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

			ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
			commandList->SetDescriptorHeaps(1, descriptorHeaps);
			commandList->SetPipelineState(graphicsPipelineState);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			D3D12_GPU_DESCRIPTOR_HANDLE currentTextureHandle = useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU1;
			D3D12_GPU_DESCRIPTOR_HANDLE currentSpriteTextureHandle = useSpriteMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU1;

			// 頂点バッファのセット
			commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

			// ルートパラメータへのリソースバインド
			// RootParameter Index [0]: マテリアル (ピクセルシェーダー用 b0)
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

			// RootParameter Index [1]: 平行光源 (ピクセルシェーダー用 b1)
			commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());

			// RootParameter Index [2]: WVP行列 (頂点シェーダー用 b0)  ←これがエラーの直接原因
			commandList->SetGraphicsRootConstantBufferView(2, wvpResource->GetGPUVirtualAddress());

			// RootParameter Index [3]: テクスチャディスクリプタテーブル (ピクセルシェーダー用 t0)
			commandList->SetGraphicsRootDescriptorTable(3, currentTextureHandle);

			commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);

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

	IDXGIDebug* debug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
	{
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_DETAIL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_DETAIL);
		debug->Release();
	}

	CloseHandle(fenceEvent);
	CloseWindow(hwnd);

#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif

	if (keyboard) {
		keyboard->Unacquire(); // 占有状態を解除
	}

	soundManager->SoundUnload(&soundData);
	soundManager->Finalize();

	CoUninitialize();
	return 0;
}