#include "GameScene.h"

// キー入力判定関数
static bool IsPushKey(uint8_t keyNumber, const BYTE* key) {
	return key[keyNumber] != 0;
}

static bool IsTriggerKey(uint8_t keyNumber, const BYTE* key, const BYTE* keyPre) {
	return (key[keyNumber] && !keyPre[keyNumber]);
}

GameScene::~GameScene() {
	if (keyboard) {
		keyboard->Unacquire();
	}
	if (soundManager) {
		soundManager->SoundUnload(&soundData);
	}
	// 動的破棄
	delete gamePad;
	gamePad = nullptr;
}

void GameScene::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, HWND hwnd, HINSTANCE hInstance) {
	// サウンドの初期化
	soundManager = Sound::GetInstance();
	soundManager->Initialize();
	soundData = soundManager->SoundLoadWave("Resources/fanfare.wav");
	soundManager->SoundPlayWave(soundData);

	// デバッグカメラの初期化
	debugCamera.Initialize();

	// 入力の初期化
	InitializeInput(hwnd, hInstance);

	// 各種バッファ・テクスチャ・モデルの読み込み
	InitializeResources(device, commandList);

	// 球体モデルデータの生成
	InitializeSphere(device);
}

void GameScene::InitializeInput(HWND hwnd, HINSTANCE hInstance) {
	HRESULT hr = DirectInput8Create(
		hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
		(void**)directInput.GetAddressOf(), nullptr);
	assert(SUCCEEDED(hr));

	hr = directInput->CreateDevice(GUID_SysKeyboard, keyboard.GetAddressOf(), NULL);
	assert(SUCCEEDED(hr));

	hr = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(hr));

	hr = keyboard->SetCooperativeLevel(
		hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));

	// new による動的生成と初期化関数の呼出し
	gamePad = new DirectInput();
	gamePad->Initialize();
}

void GameScene::InitializeResources(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
	// WVPバッファの生成
	wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	wvpData->WVP = MakeIdentity4x4();
	wvpData->World = MakeIdentity4x4();

	// マテリアルバッファの生成
	materialResource = CreateBufferResource(device, sizeof(Material));
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = 2;
	materialData->uvTransform = MakeIdentity4x4();

	// 平行光源バッファの生成
	directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;

	// テクスチャ読み込み
	textureSrvHandleGPU2 = TextureManager::LoadTextureAndCreateSRV("Resources/monsterBall.png");
	textureSrvHandleGPU3 = TextureManager::LoadTextureAndCreateSRV("Resources/uvChecker.png");

	// ModelLoader & ModelDraw の初期化
	ModelLoader::Initialize(device, commandList, textureSrvHandleGPU3);
	ModelDraw::Initialize(commandList, directionalLightResource.Get());

	// OBJモデル読み込み
	modelEntries.push_back(Model::CreateFromOBJ("suzanne", true));
	modelEntries.push_back(Model::CreateFromOBJ("player", true));
	modelEntries.push_back(Model::CreateFromOBJ("enemy", true));
	modelEntries.push_back(Model::CreateFromOBJ("teapot", true));
	modelEntries.push_back(Model::CreateFromOBJ("multiMaterial", true));
	modelEntries.push_back(Model::CreateFromOBJ("bunny", true));
	modelEntries.push_back(Model::CreateFromOBJ("multiMesh", true));

	planeModel = Model::CreateFromOBJ("plane", false);

	textureSrvHandleGPU1 = (modelEntries.empty() || modelEntries[0]->instances.empty())
		? textureSrvHandleGPU3
		: modelEntries[0]->instances[0].defaultSrvGpuHandle;

	// Sprite用リソース生成
	spriteMaterialResource = CreateBufferResource(device, sizeof(Material));
	spriteMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&spriteMaterialData));
	spriteMaterialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	spriteMaterialData->enableLighting = 0;
	spriteMaterialData->uvTransform = MakeIdentity4x4();

	spriteWvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
	spriteWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&spriteWvpData));
}

void GameScene::InitializeSphere(ID3D12Device* device) {
	const uint32_t kSubdivision = 16;
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

	sphereVertexResource = CreateBufferResource(device, sizeof(VertexData) * sphereVertices.size());
	sphereVertexBufferView.BufferLocation = sphereVertexResource->GetGPUVirtualAddress();
	sphereVertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * sphereVertices.size());
	sphereVertexBufferView.StrideInBytes = sizeof(VertexData);

	VertexData* sphereVertexData = nullptr;
	sphereVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereVertexData));
	std::memcpy(sphereVertexData, sphereVertices.data(), sizeof(VertexData) * sphereVertices.size());

	sphereMaterialResource = CreateBufferResource(device, sizeof(Material));
	sphereMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereMaterialData));
	sphereMaterialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	sphereMaterialData->enableLighting = 2;
	sphereMaterialData->uvTransform = MakeIdentity4x4();

	sphereWvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
	sphereWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereWvpData));
}

void GameScene::Update() {
	// キーボード状態の更新
	std::memcpy(keyPre, key, sizeof(key));
	keyboard->Acquire();
	keyboard->GetDeviceState(sizeof(key), key);

	// デバッグカメラ切り替え判定
	if (IsTriggerKey(DIK_TAB, key, keyPre)) {
		isDebugCamera = !isDebugCamera;
	}

	if (isDebugCamera) {
		debugCamera.Update(key); // 引数に key を渡す
		viewMatrix = debugCamera.GetViewMatrix();
		projectionMatrix = debugCamera.GetProjectionMatrix();
	}

	// ゲームパッド更新
	// アロー演算子 (->) で呼び出し
	if (gamePad) {
		gamePad->Update();
		Vector2 lStick = gamePad->GetLeftStick();
		transform.translate.x += lStick.x * 0.1f;
		transform.translate.y += lStick.y * 0.1f;
	}

	// ImGui更新
	DrawImGui();

	// Transform & 行列計算の更新
	transform.rotate.x = sphereRotate[0] * (std::numbers::pi_v<float> / 180.0f);
	transform.rotate.y = sphereRotate[1] * (std::numbers::pi_v<float> / 180.0f);
	transform.rotate.z = sphereRotate[2] * (std::numbers::pi_v<float> / 180.0f);

	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	wvpData->WVP = worldViewProjectionMatrix;
	wvpData->World = worldMatrix;

	// 平行光源の正規化
	float length = std::sqrt(uiLightDirection[0] * uiLightDirection[0] +
		uiLightDirection[1] * uiLightDirection[1] +
		uiLightDirection[2] * uiLightDirection[2]);
	if (length > 0.0f) {
		directionalLightData->direction.x = uiLightDirection[0] / length;
		directionalLightData->direction.y = uiLightDirection[1] / length;
		directionalLightData->direction.z = uiLightDirection[2] / length;
	}
	else {
		directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	}

	// 中央モデル UV Transform
	uvScale.x = uiUVScale[0];
	uvScale.y = uiUVScale[1];
	uvRotate.z = uiUVRotate * (std::numbers::pi_v<float> / 180.0f);
	uvTranslate.x = uiUVTranslate[0];
	uvTranslate.y = uiUVTranslate[1];
	materialData->uvTransform = MakeUVTransformMatrix(uvScale, uvRotate, uvTranslate);

	// Sprite Transform & UV
	spriteUVScale.x = uiSpriteUVScale[0];
	spriteUVScale.y = uiSpriteUVScale[1];
	spriteUVRotate.z = uiSpriteUVRotate * (std::numbers::pi_v<float> / 180.0f);
	spriteUVTranslate.x = uiSpriteUVTranslate[0];
	spriteUVTranslate.y = uiSpriteUVTranslate[1];
	spriteMaterialData->uvTransform = MakeUVTransformMatrix(spriteUVScale, spriteUVRotate, spriteUVTranslate);

	spriteTransform.translate.x = uiSpritePosition[0];
	spriteTransform.translate.y = uiSpritePosition[1];
	spriteTransform.scale.x = uiSpriteSize[0];
	spriteTransform.scale.y = uiSpriteSize[1];

	Matrix4x4 spriteWorldMatrix = MakeAffineMatrix(spriteTransform.scale, spriteTransform.rotate, spriteTransform.translate);
	Matrix4x4 sprite2DViewMatrix = MakeIdentity4x4();
	Matrix4x4 sprite2DProjectionMatrix = MakeOrthographicMatrix(0.0f, 1280.0f, 0.0f, 720.0f, 0.0f, 100.0f);
	spriteWvpData->WVP = Multiply(spriteWorldMatrix, Multiply(sprite2DViewMatrix, sprite2DProjectionMatrix));
	spriteWvpData->World = spriteWorldMatrix;

	// Sphere Transform & UV
	Matrix4x4 sphereWorldMatrix = MakeAffineMatrix(sphereTransform.scale, sphereTransform.rotate, sphereTransform.translate);
	sphereWvpData->WVP = Multiply(sphereWorldMatrix, Multiply(viewMatrix, projectionMatrix));
	sphereWvpData->World = sphereWorldMatrix;

	sphereUVScale.x = uiSphereUVScale[0];
	sphereUVScale.y = uiSphereUVScale[1];
	sphereUVRotate.z = uiSphereUVRotate * (std::numbers::pi_v<float> / 180.0f);
	sphereUVTranslate.x = uiSphereUVTranslate[0];
	sphereUVTranslate.y = uiSphereUVTranslate[1];
	sphereMaterialData->uvTransform = MakeUVTransformMatrix(sphereUVScale, sphereUVRotate, sphereUVTranslate);

	// 全OBJモデルの行列・UV演算反映
	for (auto& model : modelEntries) {
		for (auto& inst : model->instances) {
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
}

void GameScene::DrawImGui() {
#ifdef USE_IMGUI
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// ★コンボボックス用のブレンドモード名リスト
	static const char* blendModeNames[] = {
		"None (なし)",
		"Normal (通常α)",
		"Add (加算)",
		"Subtract (減算)",
		"Multiply (乗算)",
		"Screen (スクリーン)"
	};

	ImGui::Begin("Scene Control Panel");
	if (ImGui::BeginTabBar("SceneControlTabBar")) {
		// 0. Sound
		if (ImGui::BeginTabItem("Sound")) {
			ImGui::Text("Audio Control Panel");
			ImGui::Separator();
			if (ImGui::Button("Play Fanfare", ImVec2(120, 30))) {
				soundManager->SoundPlayWave(soundData);
			}
			ImGui::EndTabItem();
		}

		// 1. Light
		if (ImGui::BeginTabItem("Global Light")) {
			ImGui::Text("Directional Light Settings");
			ImGui::Separator();
			ImGui::ColorEdit4("Light Color", &directionalLightData->color.x);
			ImGui::SliderFloat3("Light Direction", uiLightDirection, -1.0f, 1.0f);
			ImGui::SliderFloat("Light Intensity", &directionalLightData->intensity, 0.0f, 5.0f);
			ImGui::EndTabItem();
		}

		// 2. OBJ Models
		if (ImGui::BeginTabItem("OBJ Models")) {
			const char* lightingTypes[] = { "None (0)", "Lambert (1)", "Half Lambert (2)" };
			for (size_t mIdx = 0; mIdx < modelEntries.size(); ++mIdx) {
				ImGui::PushID(static_cast<int>(mIdx));
				if (ImGui::TreeNode(modelEntries[mIdx]->name.c_str())) {
					for (size_t subIdx = 0; subIdx < modelEntries[mIdx]->instances.size(); ++subIdx) {
						ImGui::PushID(static_cast<int>(subIdx));
						auto& inst = modelEntries[mIdx]->instances[subIdx];
						std::string meshLabel = std::format("Mesh {}", subIdx);

						if (ImGui::TreeNode(meshLabel.c_str())) {
							ImGui::Checkbox("Visible", &inst.visible);

							// ★【追加】OBJモデル インスタンスごとの Blend Mode UI
							int instBlendIdx = static_cast<int>(inst.blendMode);
							if (ImGui::Combo("Blend Mode", &instBlendIdx, blendModeNames, IM_ARRAYSIZE(blendModeNames))) {
								inst.blendMode = static_cast<BlendMode>(instBlendIdx);
							}

							if (inst.materialData && ImGui::CollapsingHeader("Material & Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
								ImGui::ColorEdit4("Material Color", &inst.materialData->color.x);
								int instLighting = inst.materialData->enableLighting;
								if (ImGui::Combo("Lighting Mode", &instLighting, lightingTypes, static_cast<int>(std::size(lightingTypes)))) {
									inst.materialData->enableLighting = instLighting;
								}
							}
							if (ImGui::CollapsingHeader("Transform")) {
								ImGui::SliderFloat3("Position", &inst.transform.translate.x, -10.0f, 10.0f);
								ImGui::SliderFloat3("Rotate", &inst.transform.rotate.x, -3.14f, 3.14f);
								ImGui::SliderFloat3("Scale", &inst.transform.scale.x, 0.01f, 5.0f);
							}
							if (ImGui::CollapsingHeader("UV Transform")) {
								ImGui::SliderFloat2("UV Scale", inst.uiUVScale, 0.1f, 10.0f);
								ImGui::SliderFloat("UV Rotate", &inst.uiUVRotate, -360.0f, 360.0f);
								ImGui::SliderFloat2("UV Translate", inst.uiUVTranslate, -5.0f, 5.0f);
							}
							if (ImGui::CollapsingHeader("Texture Choice")) {
								ImGui::RadioButton("Default MTL", &inst.textureIndex, 0); ImGui::SameLine();
								ImGui::RadioButton("MonsterBall", &inst.textureIndex, 1); ImGui::SameLine();
								ImGui::RadioButton("uvChecker", &inst.textureIndex, 2);
							}
							ImGui::TreePop();
						}
						ImGui::PopID();
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			ImGui::EndTabItem();
		}

		// 3. Central Model
		if (ImGui::BeginTabItem("Central Model")) {
			ImGui::Checkbox("Show Central Model", &showModel);
			if (showModel) {
				
				// ★【追加】中央モデル用 Blend Mode UI
				int planeBlendIdx = static_cast<int>(planeBlendMode_);
				if (ImGui::Combo("Blend Mode", &planeBlendIdx, blendModeNames, IM_ARRAYSIZE(blendModeNames))) {
					planeBlendMode_ = static_cast<BlendMode>(planeBlendIdx);
				}

				const char* lightingTypes[] = { "None (0)", "Lambert (1)", "Half Lambert (2)" };
				if (ImGui::CollapsingHeader("Material & Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::ColorEdit4("Material Color", &materialData->color.x);
					int lightingType = materialData->enableLighting;
					if (ImGui::Combo("Lighting Mode", &lightingType, lightingTypes, static_cast<int>(std::size(lightingTypes)))) {
						materialData->enableLighting = lightingType;
					}
				}
				if (ImGui::CollapsingHeader("Transform & UV")) {
					ImGui::SliderFloat3("Rotate", sphereRotate, 0.0f, 360.0f);
					ImGui::SliderFloat2("UV Scale", uiUVScale, 0.1f, 10.0f);
					ImGui::SliderFloat("UV Rotate", &uiUVRotate, -360.0f, 360.0f);
					ImGui::SliderFloat2("UV Translate", uiUVTranslate, -5.0f, 5.0f);
				}
				if (ImGui::CollapsingHeader("Texture Select")) {
					ImGui::Checkbox("use MonsterBall Texture", &useMonsterBall);
				}
			}
			ImGui::EndTabItem();
		}

		// 4. Sphere
		if (ImGui::BeginTabItem("Sphere")) {
			ImGui::Checkbox("Show Sphere", &showSphere);
			if (showSphere) {
				// ★【追加】球体用 Blend Mode UI
				int sphereBlendIdx = static_cast<int>(sphereBlendMode_);
				if (ImGui::Combo("Blend Mode", &sphereBlendIdx, blendModeNames, IM_ARRAYSIZE(blendModeNames))) {
					sphereBlendMode_ = static_cast<BlendMode>(sphereBlendIdx);
				}

				const char* lightingTypes[] = { "None (0)", "Lambert (1)", "Half Lambert (2)" };
				if (ImGui::CollapsingHeader("Material & Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::ColorEdit4("Material Color", &sphereMaterialData->color.x);
					int sphereLighting = sphereMaterialData->enableLighting;
					if (ImGui::Combo("Lighting Mode", &sphereLighting, lightingTypes, static_cast<int>(std::size(lightingTypes)))) {
						sphereMaterialData->enableLighting = sphereLighting;
					}
				}
				if (ImGui::CollapsingHeader("Transform & UV")) {
					ImGui::SliderFloat3("Scale", &sphereTransform.scale.x, 0.1f, 10.0f);
					ImGui::SliderFloat3("Translate", &sphereTransform.translate.x, -10.0f, 10.0f);
					ImGui::SliderFloat2("UV Scale", uiSphereUVScale, 0.1f, 10.0f);
					ImGui::SliderFloat("UV Rotate", &uiSphereUVRotate, -360.0f, 360.0f);
					ImGui::SliderFloat2("UV Translate", uiSphereUVTranslate, -5.0f, 5.0f);
				}
				if (ImGui::CollapsingHeader("Texture Select")) {
					ImGui::RadioButton("Sphere: uvChecker", &sphereTextureIndex, 0); ImGui::SameLine();
					ImGui::RadioButton("Sphere: MonsterBall", &sphereTextureIndex, 1); ImGui::SameLine();
					ImGui::RadioButton("Sphere: Model Texture", &sphereTextureIndex, 2);
				}
			}
			ImGui::EndTabItem();
		}

		// 5. Sprite
		if (ImGui::BeginTabItem("Sprite")) {
			ImGui::Checkbox("Show Sprite", &showSprite);
			if (showSprite) {
				
				// ★【追加】スプライト用 Blend Mode UI
				int spriteBlendIdx = static_cast<int>(spriteBlendMode_);
				if (ImGui::Combo("Blend Mode", &spriteBlendIdx, blendModeNames, IM_ARRAYSIZE(blendModeNames))) {
					spriteBlendMode_ = static_cast<BlendMode>(spriteBlendIdx);
				}

				if (ImGui::CollapsingHeader("Material & Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::ColorEdit4("Material Color", &spriteMaterialData->color.x);
					const char* lightingTypes[] = { "None (0)", "Lambert (1)", "Half Lambert (2)" };
					int spriteLighting = spriteMaterialData->enableLighting;
					if (ImGui::Combo("Lighting Mode", &spriteLighting, lightingTypes, static_cast<int>(std::size(lightingTypes)))) {
						spriteMaterialData->enableLighting = spriteLighting;
					}
				}
				if (ImGui::CollapsingHeader("Screen Position & Size")) {
					ImGui::SliderFloat2("Position (Pixel)", uiSpritePosition, 0.0f, 1280.0f);
					ImGui::SliderFloat2("Size (Pixel)", uiSpriteSize, 1.0f, 1280.0f);
				}
				if (ImGui::CollapsingHeader("UV Transform")) {
					ImGui::SliderFloat2("UV Scale", uiSpriteUVScale, 0.1f, 10.0f);
					ImGui::SliderFloat("UV Rotate", &uiSpriteUVRotate, -360.0f, 360.0f);
					ImGui::SliderFloat2("UV Translate", uiSpriteUVTranslate, -5.0f, 5.0f);
				}
				if (ImGui::CollapsingHeader("Texture Select")) {
					ImGui::RadioButton("Sprite: uvChecker", &spriteTextureIndex, 0); ImGui::SameLine();
					ImGui::RadioButton("Sprite: MonsterBall", &spriteTextureIndex, 1); ImGui::SameLine();
					ImGui::RadioButton("Sprite: Model Texture", &spriteTextureIndex, 2);
				}
			}
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
	ImGui::End();
	ImGui::Render();
#endif
}

void GameScene::Draw(ID3D12GraphicsCommandList* commandList) {

	// OBJモデル群の描画
	for (auto& model : modelEntries) {
		Model::PreDraw();
		model->Draw();
		Model::PostDraw();
	}

	// 1. 中央モデル (Plane) の描画
	if (showModel && planeModel && !planeModel->instances.empty()) {
		// ★中央モデル描画直前にパイプラインをセット
		BlendManager::GetInstance()->SetPipelineState(commandList, planeBlendMode_);

		auto& inst = planeModel->instances[0];
		D3D12_GPU_DESCRIPTOR_HANDLE currentTextureHandle = useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU1;
		commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(2, wvpResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(3, currentTextureHandle);
		commandList->IASetVertexBuffers(0, 1, &inst.vertexBufferView);
		commandList->DrawInstanced(inst.vertexCount, 1, 0, 0);
	}

	// 2. 球 (Sphere) の描画
	if (showSphere) {
		// ★球体描画直前にパイプラインをセット
		BlendManager::GetInstance()->SetPipelineState(commandList, sphereBlendMode_);

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
		commandList->DrawInstanced(static_cast<UINT>(sphereVertices.size()), 1, 0, 0);
	}

	// 3. Sprite の描画
	if (showSprite && planeModel && !planeModel->instances.empty()) {
		// ★スプライト描画直前にパイプラインをセット
		BlendManager::GetInstance()->SetPipelineState(commandList, spriteBlendMode_);

		auto& inst = planeModel->instances[0];
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
		commandList->IASetVertexBuffers(0, 1, &inst.vertexBufferView);
		commandList->DrawInstanced(inst.vertexCount, 1, 0, 0);
	}

#ifdef USE_IMGUI
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}