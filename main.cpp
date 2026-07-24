#include "Common.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include <numbers>
#include "ResourceObject.h"
#include <wrl.h>
#include "Sound.h"
#include "DebugCamera.h"
#include "DirectInput.h"
#pragma comment(lib, "dxgi.lib")

// DXGIファクトリーの実体定義
IDXGIFactory7* dxgiFactory = nullptr;
ID3D12Device* device = nullptr;

BYTE key[256] = {};     // 現在のフレームのキー状態
BYTE keyPre[256] = {};  // 1フレーム前のキー状態

// キー入力判定関数群
bool IsPushKey(uint8_t keyNumber, const BYTE* key) {
	return key[keyNumber] != 0;
}

bool IsReleaseKey(uint8_t keyNumber, const BYTE* key) {
	return key[keyNumber] == 0;
}

bool IsTriggerKey(uint8_t keyNumber, const BYTE* key, const BYTE* keyPre) {
	return (key[keyNumber] && !keyPre[keyNumber]);
}

bool IsExitKey(uint8_t keyNumber, const BYTE* key, const BYTE* keyPre) {
	return (!key[keyNumber] && keyPre[keyNumber]);
}

// 個別モデル描画用内部データ構造体
struct RenderMeshInstance {
	ID3D12Resource* vertexResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	UINT vertexCount = 0;
	ID3D12Resource* materialResource = nullptr;
	Material* materialData = nullptr;
	ID3D12Resource* wvpResource = nullptr;
	TransformationMatrix* wvpData = nullptr;

	struct Transform transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Vector3 uvScale = { 1.0f, 1.0f, 1.0f };
	Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
	Vector3 uvTranslate = { 0.0f, 0.0f, 0.0f };

	float uiUVScale[2] = { 1.0f, 1.0f };
	float uiUVRotate = 0.0f;
	float uiUVTranslate[2] = { 0.0f, 0.0f };

	int textureIndex = 0; // SRVインデックスまたはテクスチャ識別子
	D3D12_GPU_DESCRIPTOR_HANDLE defaultSrvGpuHandle{};
	bool visible = true;
};

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
	CoInitializeEx(0, COINIT_MULTITHREADED);
	SetUnhandledExceptionFilter(ExportDump);

	OutputDebugStringA("Hello, DirectX!\n");

	// サウンドシステムの初期化
	Sound* soundManager = Sound::GetInstance();
	soundManager->Initialize();

	Sound::SoundData soundData = soundManager->SoundLoadWave("Resources/fanfare.wav");
	soundManager->SoundPlayWave(soundData);

	DirectInput gamePad;

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

	device = nullptr;
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

	// DirectInputの初期化
	ResourceObject<IDirectInput8> directInput;
	hr = DirectInput8Create(
		hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
		(void**)directInput.GetAddressOf(), nullptr);
	assert(SUCCEEDED(hr));

	ResourceObject<IDirectInputDevice8> keyboard;
	hr = directInput->CreateDevice(GUID_SysKeyboard, keyboard.GetAddressOf(), NULL);
	assert(SUCCEEDED(hr));

	hr = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(hr));

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

	ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Material));
	Material* materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = 1;
	materialData->uvTransform = MakeIdentity4x4();

	ID3D12Resource* directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
	DirectionalLight* directionalLightData = nullptr;
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;

#pragma endregion

	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	uint32_t srvIndexCounter = 0;

#pragma region Textureの読み込みとSRVの作成

	DirectX::ScratchImage mipImages2 = LoadTexture("Resources/monsterBall.png");
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 = CreateTextureResource(device, mipImages2.GetMetadata());
	UploadTextureData(textureResource2.Get(), mipImages2, device, commandList);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSize, srvIndexCounter);
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = mipImages2.GetMetadata().format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = UINT(mipImages2.GetMetadata().mipLevels);
		device->CreateShaderResourceView(textureResource2.Get(), &srvDesc, GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSize, srvIndexCounter++));
	}

	DirectX::ScratchImage uvCheckerImages = LoadTexture("Resources/uvChecker.png");
	Microsoft::WRL::ComPtr<ID3D12Resource> uvCheckerResource = CreateTextureResource(device, uvCheckerImages.GetMetadata());
	UploadTextureData(uvCheckerResource.Get(), uvCheckerImages, device, commandList);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3 = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSize, srvIndexCounter);
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = uvCheckerImages.GetMetadata().format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = UINT(uvCheckerImages.GetMetadata().mipLevels);
		device->CreateShaderResourceView(uvCheckerResource.Get(), &srvDesc, GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSize, srvIndexCounter++));
	}

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

#pragma endregion

	

#pragma region 全モデルデータの読み込み・リソース生成

	struct ModelEntry {
		std::string name;
		std::string dir;
		std::string file;
		std::vector<RenderMeshInstance> instances;
	};

	std::vector<ModelEntry> modelEntries = {
	{ "planeOBJ",         "Resources",              "plane.obj" },
	{ "multiMaterialOBJ", "Resources/multiMaterial", "multiMaterial.obj" },
	{ "multiMeshOBJ",     "Resources/multiMesh",     "multiMesh.obj" },
	{ "teapotOBJ",        "Resources/teapot",        "teapot.obj" },
	{ "bunnyOBJ",         "Resources/bunny",         "bunny.obj" },
	{ "suzanneOBJ",       "Resources/suzanne",       "suzanne.obj" },
	{ "playerOBJ",       "Resources/player",       "player.obj" },
	};

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> loadedTextureResources;

	for (size_t modelIdx = 0; modelIdx < modelEntries.size(); ++modelIdx)
	{
		ModelData mData = LoadObjFile(modelEntries[modelIdx].dir, modelEntries[modelIdx].file);
		for (size_t meshIdx = 0; meshIdx < mData.meshes.size(); ++meshIdx)
		{
			RenderMeshInstance inst;
			inst.vertexCount = static_cast<UINT>(mData.meshes[meshIdx].vertices.size());
			if (inst.vertexCount == 0) continue;

			// 頂点バッファ
			inst.vertexResource = CreateBufferResource(device, sizeof(VertexData) * inst.vertexCount);
			inst.vertexBufferView.BufferLocation = inst.vertexResource->GetGPUVirtualAddress();
			inst.vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * inst.vertexCount);
			inst.vertexBufferView.StrideInBytes = sizeof(VertexData);
			VertexData* vData = nullptr;
			inst.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vData));
			std::memcpy(vData, mData.meshes[meshIdx].vertices.data(), sizeof(VertexData) * inst.vertexCount);

			// マテリアル定数バッファ (修正ポイント: Map処理を追加)
			inst.materialResource = CreateBufferResource(device, sizeof(Material));
			inst.materialResource->Map(0, nullptr, reinterpret_cast<void**>(&inst.materialData));
			if (inst.materialData) {
				inst.materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
				inst.materialData->enableLighting = 2;
				inst.materialData->uvTransform = MakeIdentity4x4();
			}

			// WVP定数バッファ
			inst.wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
			inst.wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&inst.wvpData));
			if (inst.wvpData) {
				inst.wvpData->WVP = MakeIdentity4x4();
				inst.wvpData->World = MakeIdentity4x4();
			}

			// テクスチャロード
			D3D12_GPU_DESCRIPTOR_HANDLE gpuSrvHandle = textureSrvHandleGPU3; // デフォルト: uvChecker
			std::string texPath = mData.meshes[meshIdx].material.textureFilePath;
			if (!texPath.empty() && std::filesystem::exists(texPath))
			{
				DirectX::ScratchImage mTexImages = LoadTexture(texPath);
				Microsoft::WRL::ComPtr<ID3D12Resource> tResource = CreateTextureResource(device, mTexImages.GetMetadata());
				UploadTextureData(tResource.Get(), mTexImages, device, commandList);

				gpuSrvHandle = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSize, srvIndexCounter);
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
				srvDesc.Format = mTexImages.GetMetadata().format;
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MipLevels = UINT(mTexImages.GetMetadata().mipLevels);
				device->CreateShaderResourceView(tResource.Get(), &srvDesc, GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSize, srvIndexCounter++));

				loadedTextureResources.push_back(tResource);
			}

			inst.defaultSrvGpuHandle = gpuSrvHandle;
			inst.transform.translate = {
				static_cast<float>(modelIdx) * 0.0f,
				0.0f,
				static_cast<float>(meshIdx) * 0.0f
			};

			modelEntries[modelIdx].instances.push_back(inst);
		}
	}

	ModelData modelData = LoadObjFile("Resources", "plane.obj");
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU1 = modelEntries[0].instances.empty() ? textureSrvHandleGPU3 : modelEntries[0].instances[0].defaultSrvGpuHandle;

#pragma endregion

#ifdef USE_IMGUI

	D3D12_CPU_DESCRIPTOR_HANDLE imguiSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSize, srvIndexCounter);
	D3D12_GPU_DESCRIPTOR_HANDLE imguiSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSize, srvIndexCounter++);

#endif

	ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * (modelData.meshes.empty() ? 0 : modelData.meshes[0].vertices.size()));
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	if (!modelData.meshes.empty()) {
		vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
		vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * modelData.meshes[0].vertices.size());
		vertexBufferView.StrideInBytes = sizeof(VertexData);
		VertexData* vertexData = nullptr;
		vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
		std::memcpy(vertexData, modelData.meshes[0].vertices.data(), sizeof(VertexData) * modelData.meshes[0].vertices.size());
	}

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
		imguiSrvHandleCPU,
		imguiSrvHandleGPU);
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

	Vector3 uvScale = { 1.0f, 1.0f, 1.0f };
	Vector3 uvRotate = { 0.0f, 0.0f, 0.0f };
	Vector3 uvTranslate = { 0.0f, 0.0f, 0.0f };

	float uiUVScale[2] = { 1.0f, 1.0f };
	float uiUVRotate = 0.0f;
	float uiUVTranslate[2] = { 0.0f, 0.0f };

	Vector3 spriteUVScale = { 1.0f, 1.0f, 1.0f };
	Vector3 spriteUVRotate = { 0.0f, 0.0f, 0.0f };
	Vector3 spriteUVTranslate = { 0.0f, 0.0f, 0.0f };

	float uiSpriteUVScale[2] = { 1.0f, 1.0f };
	float uiSpriteUVRotate = 0.0f;
	float uiSpriteUVTranslate[2] = { 0.0f, 0.0f };

	float uiSpritePosition[2] = { 320.0f, 180.0f };
	float uiSpriteSize[2] = { 320.0f, 180.0f };

	ID3D12Resource* spriteMaterialResource = CreateBufferResource(device, sizeof(Material));
	Material* spriteMaterialData = nullptr;
	spriteMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&spriteMaterialData));
	spriteMaterialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	spriteMaterialData->enableLighting = 0;
	spriteMaterialData->uvTransform = MakeIdentity4x4();

	ID3D12Resource* spriteWvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix* spriteWvpData = nullptr;
	spriteWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&spriteWvpData));

	struct Transform spriteTransform = {
		{ 300.0f, 300.0f, 1.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 100.0f, 100.0f, 0.0f }
	};

	Matrix4x4 spriteWorldMatrix = MakeAffineMatrix(spriteTransform.scale, spriteTransform.rotate, spriteTransform.translate);
	Matrix4x4 identityMatrix = MakeIdentity4x4();
	spriteWvpData->WVP = Multiply(spriteWorldMatrix, Multiply(identityMatrix, identityMatrix));
	spriteWvpData->World = spriteWorldMatrix;

	const uint32_t kSubdivision = 16;

	bool showModel = true;
	bool showSprite = true;
	bool showSphere = true;

	static int sphereTextureIndex = 0;

	struct Transform sphereTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Vector3 sphereUVScale = { 1.0f, 1.0f, 1.0f };
	Vector3 sphereUVRotate = { 0.0f, 0.0f, 0.0f };
	Vector3 sphereUVTranslate = { 0.0f, 0.0f, 0.0f };

	float uiSphereUVScale[2] = { 1.0f, 1.0f };
	float uiSphereUVRotate = 0.0f;
	float uiSphereUVTranslate[2] = { 0.0f, 0.0f };

	std::vector<VertexData> sphereVertices;
	for (uint32_t lat = 0; lat < kSubdivision; ++lat) {
		float lat0 = std::numbers::pi_v<float> *(-0.5f + (float)lat / kSubdivision);
		float lat1 = std::numbers::pi_v<float> *(-0.5f + (float)(lat + 1) / kSubdivision);
		for (uint32_t lon = 0; lon < kSubdivision; ++lon) {
			float lon0 = 2.0f * std::numbers::pi_v<float> *(float)lon / kSubdivision;
			float lon1 = 2.0f * std::numbers::pi_v<float> *(float)(lon + 1) / kSubdivision;

			auto GetSphereVertex = [](float lat, float lon) -> VertexData {
				VertexData v;
				v.position.x = std::cos(lat) * std::cos(lon);
				v.position.y = std::sin(lat);
				v.position.z = std::cos(lat) * std::sin(lon);
				v.position.w = 1.0f;
				v.normal = { v.position.x, v.position.y, v.position.z };
				v.u = lon / (2.0f * std::numbers::pi_v<float>);
				v.v = 1.0f - (lat / std::numbers::pi_v<float> +0.5f);
				return v;
				};

			VertexData v0 = GetSphereVertex(lat0, lon0);
			VertexData v1 = GetSphereVertex(lat1, lon0);
			VertexData v2 = GetSphereVertex(lat0, lon1);
			VertexData v3 = GetSphereVertex(lat1, lon1);

			sphereVertices.push_back(v0); sphereVertices.push_back(v1); sphereVertices.push_back(v2);
			sphereVertices.push_back(v1); sphereVertices.push_back(v3); sphereVertices.push_back(v2);
		}
	}

	ID3D12Resource* sphereVertexResource = CreateBufferResource(device, sizeof(VertexData) * sphereVertices.size());
	D3D12_VERTEX_BUFFER_VIEW sphereVertexBufferView{};
	sphereVertexBufferView.BufferLocation = sphereVertexResource->GetGPUVirtualAddress();
	sphereVertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * sphereVertices.size());
	sphereVertexBufferView.StrideInBytes = sizeof(VertexData);

	VertexData* sphereVertexData = nullptr;
	sphereVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereVertexData));
	std::memcpy(sphereVertexData, sphereVertices.data(), sizeof(VertexData) * sphereVertices.size());

	ID3D12Resource* sphereMaterialResource = CreateBufferResource(device, sizeof(Material));
	Material* sphereMaterialData = nullptr;
	sphereMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereMaterialData));
	sphereMaterialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	sphereMaterialData->enableLighting = 1;
	sphereMaterialData->uvTransform = MakeIdentity4x4();

	ID3D12Resource* sphereWvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix* sphereWvpData = nullptr;
	sphereWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereWvpData));

	materialData->enableLighting = 2;
	sphereMaterialData->enableLighting = 2;

	bool useMonsterBall = true;
	bool useSpriteMonsterBall = false;

	float uiLightDirection[3] = { 0.0f, -1.0f, 0.0f };
	float sphereRotate[3] = { 0.0f, 0.0f, 0.0f };

#pragma endregion

	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	ID3D12CommandList* initCommandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(1, initCommandLists);

	// GPUの完了を待機
	fenceValue++;
	commandQueue->Signal(fence, fenceValue);
	if (fence->GetCompletedValue() < fenceValue)
	{
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	// メインループ用にコマンドリストをリセット
	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList->Reset(commandAllocator, nullptr);
	assert(SUCCEEDED(hr));

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			bool isDebugCamera = false;

			// 例えば「TABキー」で切り替え
			if (IsTriggerKey(DIK_TAB, key, keyPre)) {
				isDebugCamera = !isDebugCamera;
			}

			if (isDebugCamera) {
				debugCamera.Update();
				viewMatrix = debugCamera.GetViewMatrix();
				projectionMatrix = debugCamera.GetProjectionMatrix();
			}
			else {
				// 通常のゲーム用カメラ行列の更新処理
			}

			gamePad.Update();

			// Aボタンを押した瞬間の処理
			if (gamePad.IsTrigger(XINPUT_GAMEPAD_A)) {
				OutputDebugStringA("A Button Pressed\n");
			}

			// 左スティックでオブジェクトを移動
			Vector2 lStick = gamePad.GetLeftStick();
			transform.translate.x += lStick.x * 0.1f;
			transform.translate.y += lStick.y * 0.1f;

			std::memcpy(keyPre, key, sizeof(key));
			keyboard->Acquire();
			keyboard->GetDeviceState(sizeof(key), key);

			if (IsTriggerKey(DIK_SPACE, key, keyPre))
			{
				OutputDebugStringA("Space Triggered\n");
			}

			if (IsPushKey(DIK_0, key))
			{
				OutputDebugStringA("Hit 0\n");
			}

			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			static int spriteTextureIndex = 0;

			ImGui::Begin("Texture Settings");
			ImGui::Checkbox("use MonsterBall Texture", &useMonsterBall);
			ImGui::Checkbox("Show Central Model", &showModel);
			ImGui::Checkbox("Show Sprite", &showSprite);
			ImGui::Checkbox("Show Sphere", &showSphere);

			ImGui::Checkbox("Model: use MonsterBall Texture", &useMonsterBall);

			ImGui::Separator();
			ImGui::Text("Sprite Texture Select:");
			ImGui::RadioButton("Sprite: uvChecker", &spriteTextureIndex, 0); ImGui::SameLine();
			ImGui::RadioButton("Sprite: MonsterBall", &spriteTextureIndex, 1); ImGui::SameLine();
			ImGui::RadioButton("Sprite: Model Texture", &spriteTextureIndex, 2);

			ImGui::Separator();
			ImGui::Text("Sphere Texture Select:");
			ImGui::RadioButton("Sphere: uvChecker", &sphereTextureIndex, 0); ImGui::SameLine();
			ImGui::RadioButton("Sphere: MonsterBall", &sphereTextureIndex, 1); ImGui::SameLine();
			ImGui::RadioButton("Sphere: Model Texture", &sphereTextureIndex, 2);

			ImGui::End();

			ImGui::Begin("Lighting Settings");

			// メインマテリアルカラーの編集
			ImGui::ColorEdit4("Material Color", &materialData->color.x);

			// ライティングの選択項目
			int currentLightingType = materialData->enableLighting;
			const char* lightingItems[] = { "None", "Lambert", "Half Lambert" };

			if (ImGui::Combo("Lighting", &currentLightingType, lightingItems, static_cast<int>(std::size(lightingItems))))
			{
				// 既存のオブジェクトに反映
				materialData->enableLighting = currentLightingType;
				sphereMaterialData->enableLighting = currentLightingType;

				// --- 追加: modelEntries 中の全インスタンスにも反映 ---
				for (auto& model : modelEntries)
				{
					for (auto& inst : model.instances)
					{
						if (inst.materialData)
						{
							inst.materialData->enableLighting = currentLightingType;
						}
					}
				}
			}

			ImGui::Separator();
			ImGui::ColorEdit4("Light Color", &directionalLightData->color.x);
			ImGui::SliderFloat3("Light Direction", uiLightDirection, -1.0f, 1.0f);
			ImGui::SliderFloat("Light Intensity", &directionalLightData->intensity, 0.0f, 5.0f);

			ImGui::End();

			transform.rotate.x = sphereRotate[0] * (std::numbers::pi_v<float> / 180.0f);
			transform.rotate.y = sphereRotate[1] * (std::numbers::pi_v<float> / 180.0f);
			transform.rotate.z = sphereRotate[2] * (std::numbers::pi_v<float> / 180.0f);

			debugCamera.Update();
			viewMatrix = debugCamera.GetViewMatrix();
			projectionMatrix = debugCamera.GetProjectionMatrix();

			worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
			Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
			wvpData->WVP = worldViewProjectionMatrix;
			wvpData->World = worldMatrix;

			spriteWorldMatrix = MakeAffineMatrix(spriteTransform.scale, spriteTransform.rotate, spriteTransform.translate);
			Matrix4x4 sprite2DViewMatrix = MakeIdentity4x4();
			Matrix4x4 sprite2DProjectionMatrix = MakeOrthographicMatrix(0.0f, 1280.0f, 0.0f, 720.0f, 0.0f, 100.0f);
			Matrix4x4 spriteWVPMatrix = Multiply(spriteWorldMatrix, Multiply(sprite2DViewMatrix, sprite2DProjectionMatrix));

			spriteWvpData->WVP = spriteWVPMatrix;
			spriteWvpData->World = spriteWorldMatrix;

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

			ImGui::Begin("Display Settings");

			ImGui::Separator();
			ImGui::Text("Model Transform");
			ImGui::SliderFloat3("Sphere Rotate", sphereRotate, 0.0f, 360.0f);
			ImGui::Text("--- Central Model UV ---");
			ImGui::SliderFloat2("Model UV Scale", uiUVScale, 0.1f, 10.0f);
			ImGui::SliderFloat("Model UV Rotate", &uiUVRotate, -360.0f, 360.0f);
			ImGui::SliderFloat2("Model UV Translate", uiUVTranslate, -5.0f, 5.0f);

			uvScale.x = uiUVScale[0];
			uvScale.y = uiUVScale[1];
			uvRotate.z = uiUVRotate * (std::numbers::pi_v<float> / 180.0f);
			uvTranslate.x = uiUVTranslate[0];
			uvTranslate.y = uiUVTranslate[1];

			materialData->uvTransform = MakeUVTransformMatrix(uvScale, uvRotate, uvTranslate);

			ImGui::Separator();
			ImGui::Text("Sprite UV");
			ImGui::SliderFloat2("Sprite UV Scale", uiSpriteUVScale, 0.1f, 10.0f);
			ImGui::SliderFloat("Sprite UV Rotate", &uiSpriteUVRotate, -360.0f, 360.0f);
			ImGui::SliderFloat2("Sprite UV Translate", uiSpriteUVTranslate, -5.0f, 5.0f);

			spriteUVScale.x = uiSpriteUVScale[0];
			spriteUVScale.y = uiSpriteUVScale[1];
			spriteUVRotate.z = uiSpriteUVRotate * (std::numbers::pi_v<float> / 180.0f);
			spriteUVTranslate.x = uiSpriteUVTranslate[0];
			spriteUVTranslate.y = uiSpriteUVTranslate[1];
			spriteMaterialData->uvTransform = MakeUVTransformMatrix(spriteUVScale, spriteUVRotate, spriteUVTranslate);

			ImGui::Separator();
			ImGui::Text("Sprite Screen Position");
			ImGui::SliderFloat2("Position (Pixel)", uiSpritePosition, 0.0f, 1280.0f);

			spriteTransform.translate.x = uiSpritePosition[0];
			spriteTransform.translate.y = uiSpritePosition[1];

			ImGui::Text("Sprite Screen Size");
			ImGui::SliderFloat2("Size (Pixel)", uiSpriteSize, 1.0f, 1280.0f);

			spriteTransform.scale.x = uiSpriteSize[0];
			spriteTransform.scale.y = uiSpriteSize[1];

			ImGui::Separator();
			ImGui::Text("Sphere Transform & UV");
			ImGui::SliderFloat3("Sphere Scale", &sphereTransform.scale.x, 0.1f, 10.0f);
			ImGui::SliderFloat3("Sphere Translate", &sphereTransform.translate.x, -10.0f, 10.0f);
			ImGui::SliderFloat2("Sphere UV Scale", uiSphereUVScale, 0.1f, 10.0f);
			ImGui::SliderFloat("Sphere UV Rotate", &uiSphereUVRotate, -360.0f, 360.0f);
			ImGui::SliderFloat2("Sphere UV Translate", uiSphereUVTranslate, -5.0f, 5.0f);

			Matrix4x4 sphereWorldMatrix = MakeAffineMatrix(sphereTransform.scale, sphereTransform.rotate, sphereTransform.translate);
			Matrix4x4 sphereWVPMatrix = Multiply(sphereWorldMatrix, Multiply(viewMatrix, projectionMatrix));
			sphereWvpData->WVP = sphereWVPMatrix;
			sphereWvpData->World = sphereWorldMatrix;

			sphereUVScale.x = uiSphereUVScale[0];
			sphereUVScale.y = uiSphereUVScale[1];
			sphereUVRotate.z = uiSphereUVRotate * (std::numbers::pi_v<float> / 180.0f);
			sphereUVTranslate.x = uiSphereUVTranslate[0];
			sphereUVTranslate.y = uiSphereUVTranslate[1];
			sphereMaterialData->uvTransform = MakeUVTransformMatrix(sphereUVScale, sphereUVRotate, sphereUVTranslate);

			ImGui::End();

			// 全OBJモデル個別UI設定パネル
			ImGui::Begin("OBJ Models Controller");
			for (size_t mIdx = 0; mIdx < modelEntries.size(); ++mIdx)
			{
				ImGui::PushID(static_cast<int>(mIdx));
				if (ImGui::TreeNode(modelEntries[mIdx].name.c_str()))
				{
					for (size_t subIdx = 0; subIdx < modelEntries[mIdx].instances.size(); ++subIdx)
					{
						ImGui::PushID(static_cast<int>(subIdx));
						auto& inst = modelEntries[mIdx].instances[subIdx];
						std::string meshLabel = std::format("Mesh {}", subIdx);
						if (ImGui::TreeNode(meshLabel.c_str()))
						{
							ImGui::Checkbox("Visible", &inst.visible);

							// --- 追加: 個別 Lighting & Material Color 設定 ---
							if (inst.materialData)
							{
								ImGui::Text("Material Settings");
								ImGui::ColorEdit4("Color", &inst.materialData->color.x);

								int instLighting = inst.materialData->enableLighting;
								const char* lightingTypes[] = { "None", "Lambert", "Half Lambert" };
								if (ImGui::Combo("Lighting Mode", &instLighting, lightingTypes, static_cast<int>(std::size(lightingTypes))))
								{
									inst.materialData->enableLighting = instLighting;
								}
							}
							ImGui::Separator();

							ImGui::SliderFloat3("Position", &inst.transform.translate.x, -10.0f, 10.0f);
							ImGui::SliderFloat3("Rotate", &inst.transform.rotate.x, -3.14f, 3.14f);
							ImGui::SliderFloat3("Scale", &inst.transform.scale.x, 0.01f, 5.0f);

							ImGui::Text("UV Transform");
							ImGui::SliderFloat2("UV Scale", inst.uiUVScale, 0.1f, 10.0f);
							ImGui::SliderFloat("UV Rotate", &inst.uiUVRotate, -360.0f, 360.0f);
							ImGui::SliderFloat2("UV Translate", inst.uiUVTranslate, -5.0f, 5.0f);

							ImGui::Text("Texture Choice");
							ImGui::RadioButton("Default MTL", &inst.textureIndex, 0); ImGui::SameLine();
							ImGui::RadioButton("MonsterBall", &inst.textureIndex, 1); ImGui::SameLine();
							ImGui::RadioButton("uvChecker", &inst.textureIndex, 2);

							ImGui::TreePop();
						}
						ImGui::PopID();
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			ImGui::End();

			// 行列・UV演算反映（全モデル・全インスタンスに対して毎フレーム更新）
			for (auto& model : modelEntries)
			{
				for (auto& inst : model.instances)
				{
					if (inst.materialData) {
						inst.uvScale.x = inst.uiUVScale[0];
						inst.uvScale.y = inst.uiUVScale[1];
						inst.uvRotate.z = inst.uiUVRotate * (std::numbers::pi_v<float> / 180.0f);
						inst.uvTranslate.x = inst.uiUVTranslate[0];
						inst.uvTranslate.y = inst.uiUVTranslate[1];
						inst.materialData->uvTransform = MakeUVTransformMatrix(inst.uvScale, inst.uvRotate, inst.uvTranslate);
					}

					if (inst.wvpData) {
						Matrix4x4 mWorld = MakeAffineMatrix(inst.transform.scale, inst.transform.rotate, inst.transform.translate);
						inst.wvpData->WVP = Multiply(mWorld, Multiply(viewMatrix, projectionMatrix));
						inst.wvpData->World = mWorld;
					}
				}
			}

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

			// --- 全モデルの描画ループ ---
			for (auto& model : modelEntries)
			{
				for (auto& inst : model.instances)
				{
					if (!inst.visible || inst.vertexCount == 0) { continue; }

					D3D12_GPU_DESCRIPTOR_HANDLE drawTexHandle = inst.defaultSrvGpuHandle;
					if (inst.textureIndex == 1) { drawTexHandle = textureSrvHandleGPU2; }
					else if (inst.textureIndex == 2) { drawTexHandle = textureSrvHandleGPU3; }

					commandList->SetGraphicsRootConstantBufferView(0, inst.materialResource->GetGPUVirtualAddress());
					commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());
					commandList->SetGraphicsRootConstantBufferView(2, inst.wvpResource->GetGPUVirtualAddress());
					commandList->SetGraphicsRootDescriptorTable(3, drawTexHandle);
					commandList->IASetVertexBuffers(0, 1, &inst.vertexBufferView);
					commandList->DrawInstanced(inst.vertexCount, 1, 0, 0);
				}
			}

			// --- 1. 中央モデルの描画（背景・互換用途） ---
			if (showModel && !modelData.meshes.empty())
			{
				D3D12_GPU_DESCRIPTOR_HANDLE currentTextureHandle = useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU1;
				commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(2, wvpResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(3, currentTextureHandle);
				commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
				commandList->DrawInstanced(UINT(modelData.meshes[0].vertices.size()), 1, 0, 0);
			}

			// --- 2. 球（Sphere）の描画 ---
			if (showSphere)
			{
				D3D12_GPU_DESCRIPTOR_HANDLE currentSphereTextureHandle = textureSrvHandleGPU3;
				if (sphereTextureIndex == 1) {
					currentSphereTextureHandle = textureSrvHandleGPU2;
				}
				else if (sphereTextureIndex == 2) {
					currentSphereTextureHandle = textureSrvHandleGPU1;
				}

				commandList->SetGraphicsRootConstantBufferView(0, sphereMaterialResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(2, sphereWvpResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(3, currentSphereTextureHandle);
				commandList->IASetVertexBuffers(0, 1, &sphereVertexBufferView);
				commandList->DrawInstanced(UINT(sphereVertices.size()), 1, 0, 0);
			}

			// --- 3. Sprite の描画 ---
			if (showSprite && !modelData.meshes.empty())
			{
				D3D12_GPU_DESCRIPTOR_HANDLE currentSpriteTextureHandle = textureSrvHandleGPU3;
				if (spriteTextureIndex == 1) {
					currentSpriteTextureHandle = textureSrvHandleGPU2;
				}
				else if (spriteTextureIndex == 2) {
					currentSpriteTextureHandle = textureSrvHandleGPU1;
				}

				commandList->SetGraphicsRootConstantBufferView(0, spriteMaterialResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(2, spriteWvpResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(3, currentSpriteTextureHandle);
				commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
				commandList->DrawInstanced(UINT(modelData.meshes[0].vertices.size()), 1, 0, 0);
			}

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
		keyboard->Unacquire();
	}

	soundManager->SoundUnload(&soundData);
	soundManager->Finalize();

	CoUninitialize();
	return 0;
}