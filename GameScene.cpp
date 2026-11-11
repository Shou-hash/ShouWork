#include "GameScene.h"
#include "Engine/2d/ImGuiManager.h"
#include "Engine/Base/WinApp.h"
#include "Engine/2d/TextureManager.h"
#include "Engine/Base/SrvManager.h"
#include "Engine/Base/SceneManager.h"

void GameScene::Finalize() {
	if (soundManager) {
		soundManager->SoundUnload(&soundData);
	}
	delete gamePad;
	gamePad = nullptr;
}

void GameScene::Initialize() {
	ID3D12Device* device = ctx_.dxCommon->GetDevice();
	ID3D12GraphicsCommandList* commandList = ctx_.dxCommon->GetCommandList();
	WinApp* winApp = ctx_.winApp;
	SrvManager* srvManager = ctx_.srvManager;
	IDxcUtils* dxcUtils = ctx_.dxcUtils;
	IDxcCompiler3* dxcCompiler = ctx_.dxcCompiler;
	IDxcIncludeHandler* dxcIncludeHandler = ctx_.dxcIncludeHandler;
	ID3D12RootSignature* rootSignature = ctx_.rootSignature;

	srvManager_ = srvManager;
	rootSignature_ = rootSignature;

	soundManager = Sound::GetInstance();
	soundManager->Initialize();
	soundData = soundManager->SoundLoadWave("Resources/fanfare.wav");
	soundManager->SoundPlayWave(soundData);

	debugCamera.Initialize();

	input_ = std::make_unique<Input>();
	input_->Initialize(winApp->GetHInstance(), winApp->GetHwnd());

	gamePad = new DirectInput();
	gamePad->Initialize();

	InitializeResources(device, commandList);
	InitializeSphere(device);

	particle_ = std::make_unique<Particle>();
	particle_->Initialize(device, srvManager, dxcUtils, dxcCompiler, dxcIncludeHandler);
}

void GameScene::InitializeResources(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
	wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	wvpData->WVP = MakeIdentity4x4();
	wvpData->World = MakeIdentity4x4();

	materialResource = CreateBufferResource(device, sizeof(Material));
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = 2;
	materialData->uvTransform = MakeIdentity4x4();

	directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;

	textureSrvHandleGPU2 = TextureManager::GetInstance()->LoadTextureAndCreateSRV("Resources/monsterBall.png");
	textureSrvHandleGPU3 = TextureManager::GetInstance()->LoadTextureAndCreateSRV("Resources/uvChecker.png");
	textureSrvHandleCircle = TextureManager::GetInstance()->LoadTextureAndCreateSRV("Resources/circle.png");

	ModelLoader::Initialize(device, commandList, textureSrvHandleGPU3);
	ModelDraw::Initialize(commandList, directionalLightResource.Get());

	modelEntries.push_back(Model::CreateFromOBJ("suzanne", true));
	modelEntries.push_back(Model::CreateFromOBJ("player", true));
	modelEntries.push_back(Model::CreateFromOBJ("enemy", true));
	modelEntries.push_back(Model::CreateFromOBJ("teapot", true));
	modelEntries.push_back(Model::CreateFromOBJ("multiMaterial", true));
	modelEntries.push_back(Model::CreateFromOBJ("bunny", true));
	modelEntries.push_back(Model::CreateFromOBJ("multiMesh", true));
	modelEntries.push_back(Model::CreateFromOBJ("fence", true));

	planeModel = Model::CreateFromOBJ("plane", false);

	textureSrvHandleGPU1 = (modelEntries.empty() || modelEntries[0]->instances.empty())
		? textureSrvHandleGPU3
		: modelEntries[0]->instances[0].defaultSrvGpuHandle;

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

void GameScene::SaveGameData(const std::string& filePath) {
	json j;

	// 1. 中央モデル (Plane) のパラメータ保存
	j["transform"] = transform;
	j["showModel"] = showModel;
	j["useMonsterBall"] = useMonsterBall;
	j["planeBlendMode"] = static_cast<int>(planeBlendMode_);
	j["uiUVScale"] = { uiUVScale[0], uiUVScale[1] };
	j["uiUVRotate"] = uiUVRotate;
	j["uiUVTranslate"] = { uiUVTranslate[0], uiUVTranslate[1] };

	// 2. 球体 (Sphere) のパラメータ保存
	j["sphereTransform"] = sphereTransform;
	j["showSphere"] = showSphere;
	j["sphereRotate"] = { sphereRotate[0], sphereRotate[1], sphereRotate[2] };
	j["sphereTextureIndex"] = sphereTextureIndex;
	j["sphereBlendMode"] = static_cast<int>(sphereBlendMode_);
	j["uiSphereUVScale"] = { uiSphereUVScale[0], uiSphereUVScale[1] };
	j["uiSphereUVRotate"] = uiSphereUVRotate;
	j["uiSphereUVTranslate"] = { uiSphereUVTranslate[0], uiSphereUVTranslate[1] };

	// 3. スプライト (Sprite) のパラメータ保存
	j["spriteTransform"] = spriteTransform;
	j["showSprite"] = showSprite;
	j["spriteTextureIndex"] = spriteTextureIndex;
	j["spriteBlendMode"] = static_cast<int>(spriteBlendMode_);
	j["uiSpritePosition"] = { uiSpritePosition[0], uiSpritePosition[1] };
	j["uiSpriteSize"] = { uiSpriteSize[0], uiSpriteSize[1] };
	j["uiSpriteUVScale"] = { uiSpriteUVScale[0], uiSpriteUVScale[1] };
	j["uiSpriteUVRotate"] = uiSpriteUVRotate;
	j["uiSpriteUVTranslate"] = { uiSpriteUVTranslate[0], uiSpriteUVTranslate[1] };

	// 4. ライトのパラメータ保存
	j["uiLightDirection"] = { uiLightDirection[0], uiLightDirection[1], uiLightDirection[2] };
	if (directionalLightData) {
		j["lightColor"] = Vector4{ directionalLightData->color.x, directionalLightData->color.y, directionalLightData->color.z, directionalLightData->color.w };
		j["lightIntensity"] = directionalLightData->intensity;
	}

	std::ofstream file(filePath);
	if (file.is_open()) {
		file << j.dump(4);
	}
}

void GameScene::LoadGameData(const std::string& filePath) {
	std::ifstream file(filePath);
	if (!file.is_open()) {
		return;
	}

	json j;
	file >> j;

	// 1. 中央モデル (Plane) 復元
	if (j.contains("transform")) transform = j["transform"].get<Transform>();
	if (j.contains("showModel")) showModel = j["showModel"].get<bool>();
	if (j.contains("useMonsterBall")) useMonsterBall = j["useMonsterBall"].get<bool>();
	if (j.contains("planeBlendMode")) planeBlendMode_ = static_cast<BlendMode>(j["planeBlendMode"].get<int>());
	if (j.contains("uiUVScale")) { auto v = j["uiUVScale"].get<std::vector<float>>(); if (v.size() >= 2) { uiUVScale[0] = v[0]; uiUVScale[1] = v[1]; } }
	if (j.contains("uiUVRotate")) uiUVRotate = j["uiUVRotate"].get<float>();
	if (j.contains("uiUVTranslate")) { auto v = j["uiUVTranslate"].get<std::vector<float>>(); if (v.size() >= 2) { uiUVTranslate[0] = v[0]; uiUVTranslate[1] = v[1]; } }

	// 2. 球体 (Sphere) 復元
	if (j.contains("sphereTransform")) sphereTransform = j["sphereTransform"].get<Transform>();
	if (j.contains("showSphere")) showSphere = j["showSphere"].get<bool>();
	if (j.contains("sphereRotate")) { auto v = j["sphereRotate"].get<std::vector<float>>(); if (v.size() >= 3) { sphereRotate[0] = v[0]; sphereRotate[1] = v[1]; sphereRotate[2] = v[2]; } }
	if (j.contains("sphereTextureIndex")) sphereTextureIndex = j["sphereTextureIndex"].get<int>();
	if (j.contains("sphereBlendMode")) sphereBlendMode_ = static_cast<BlendMode>(j["sphereBlendMode"].get<int>());
	if (j.contains("uiSphereUVScale")) { auto v = j["uiSphereUVScale"].get<std::vector<float>>(); if (v.size() >= 2) { uiSphereUVScale[0] = v[0]; uiSphereUVScale[1] = v[1]; } }
	if (j.contains("uiSphereUVRotate")) uiSphereUVRotate = j["uiSphereUVRotate"].get<float>();
	if (j.contains("uiSphereUVTranslate")) { auto v = j["uiSphereUVTranslate"].get<std::vector<float>>(); if (v.size() >= 2) { uiSphereUVTranslate[0] = v[0]; uiSphereUVTranslate[1] = v[1]; } }

	// 3. スプライト (Sprite) 復元
	if (j.contains("spriteTransform")) spriteTransform = j["spriteTransform"].get<Transform>();
	if (j.contains("showSprite")) showSprite = j["showSprite"].get<bool>();
	if (j.contains("spriteTextureIndex")) spriteTextureIndex = j["spriteTextureIndex"].get<int>();
	if (j.contains("spriteBlendMode")) spriteBlendMode_ = static_cast<BlendMode>(j["spriteBlendMode"].get<int>());
	if (j.contains("uiSpritePosition")) { auto v = j["uiSpritePosition"].get<std::vector<float>>(); if (v.size() >= 2) { uiSpritePosition[0] = v[0]; uiSpritePosition[1] = v[1]; } }
	if (j.contains("uiSpriteSize")) { auto v = j["uiSpriteSize"].get<std::vector<float>>(); if (v.size() >= 2) { uiSpriteSize[0] = v[0]; uiSpriteSize[1] = v[1]; } }
	if (j.contains("uiSpriteUVScale")) { auto v = j["uiSpriteUVScale"].get<std::vector<float>>(); if (v.size() >= 2) { uiSpriteUVScale[0] = v[0]; uiSpriteUVScale[1] = v[1]; } }
	if (j.contains("uiSpriteUVRotate")) uiSpriteUVRotate = j["uiSpriteUVRotate"].get<float>();
	if (j.contains("uiSpriteUVTranslate")) { auto v = j["uiSpriteUVTranslate"].get<std::vector<float>>(); if (v.size() >= 2) { uiSpriteUVTranslate[0] = v[0]; uiSpriteUVTranslate[1] = v[1]; } }

	// 4. ライト復元
	if (j.contains("uiLightDirection")) { auto v = j["uiLightDirection"].get<std::vector<float>>(); if (v.size() >= 3) { uiLightDirection[0] = v[0]; uiLightDirection[1] = v[1]; uiLightDirection[2] = v[2]; } }
	if (directionalLightData) {
		if (j.contains("lightColor")) { auto col = j["lightColor"].get<Vector4>(); directionalLightData->color = { col.x, col.y, col.z, col.w }; }
		if (j.contains("lightIntensity")) directionalLightData->intensity = j["lightIntensity"].get<float>();
	}
}

void GameScene::Update() {
	input_->Update();

	if (input_->TriggerKey(DIK_S)) {
		SaveGameData();
	}
	if (input_->TriggerKey(DIK_L)) {
		LoadGameData();
	}

	if (input_->TriggerKey(DIK_ESCAPE)) {
		SceneManager::GetInstance()->ChangeScene("TITLE");
	}

	if (input_->TriggerKey(DIK_TAB)) {
		isDebugCamera = !isDebugCamera;
	}

	if (isDebugCamera) {
		debugCamera.Update(input_.get());
		viewMatrix = debugCamera.GetViewMatrix();
		projectionMatrix = debugCamera.GetProjectionMatrix();
	}

	if (gamePad) {
		gamePad->Update();
		Vector2 lStick = gamePad->GetLeftStick();
		transform.translate.x += lStick.x * 0.1f;
		transform.translate.y += lStick.y * 0.1f;
	}

	DrawImGui();

	spriteUVScale.x = textureCutSize.x / textureSize.x;
	spriteUVScale.y = textureCutSize.y / textureSize.y;
	spriteUVTranslate.x = textureLeftTop.x / textureSize.x;
	spriteUVTranslate.y = textureLeftTop.y / textureSize.y;
	spriteMaterialData->uvTransform = MakeUVTransformMatrix(spriteUVScale, spriteUVRotate, spriteUVTranslate);

	if (particle_ && showParticles) {
		particle_->Update(ctx_.dxCommon->GetCommandList(), viewMatrix, projectionMatrix);
	}

	transform.rotate.x = sphereRotate[0] * (std::numbers::pi_v<float> / 180.0f);
	transform.rotate.y = sphereRotate[1] * (std::numbers::pi_v<float> / 180.0f);
	transform.rotate.z = sphereRotate[2] * (std::numbers::pi_v<float> / 180.0f);

	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	wvpData->WVP = worldViewProjectionMatrix;
	wvpData->World = worldMatrix;

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

	uvScale.x = uiUVScale[0];
	uvScale.y = uiUVScale[1];
	uvRotate.z = uiUVRotate * (std::numbers::pi_v<float> / 180.0f);
	uvTranslate.x = uiUVTranslate[0];
	uvTranslate.y = uiUVTranslate[1];
	materialData->uvTransform = MakeUVTransformMatrix(uvScale, uvRotate, uvTranslate);

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

	Matrix4x4 sphereWorldMatrix = MakeAffineMatrix(sphereTransform.scale, sphereTransform.rotate, sphereTransform.translate);
	sphereWvpData->WVP = Multiply(sphereWorldMatrix, Multiply(viewMatrix, projectionMatrix));
	sphereWvpData->World = sphereWorldMatrix;

	sphereUVScale.x = uiSphereUVScale[0];
	sphereUVScale.y = uiSphereUVScale[1];
	sphereUVRotate.z = uiSphereUVRotate * (std::numbers::pi_v<float> / 180.0f);
	sphereUVTranslate.x = uiSphereUVTranslate[0];
	sphereUVTranslate.y = uiSphereUVTranslate[1];
	sphereMaterialData->uvTransform = MakeUVTransformMatrix(sphereUVScale, sphereUVRotate, sphereUVTranslate);

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
	static const char* blendModeNames[] = {
		"None (なし)",
		"Normal (通常α)",
		"Add (加算)",
		"Subtract (減算)",
		"Multiply (乗算)",
		"Screen (スクリーン)"
	};

	ImGui::Begin("Scene Control Panel");

	if (ImGui::Button("Save Data", ImVec2(100, 25))) {
		SaveGameData();
	}
	ImGui::SameLine();
	if (ImGui::Button("Load Data", ImVec2(100, 25))) {
		LoadGameData();
	}
	ImGui::Separator();

	if (ImGui::BeginTabBar("SceneControlTabBar")) {
		if (ImGui::BeginTabItem("Sound")) {
			ImGui::Text("Audio Control Panel");
			ImGui::Separator();
			if (ImGui::Button("Play Fanfare", ImVec2(120, 30))) {
				soundManager->SoundPlayWave(soundData);
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Global Light")) {
			ImGui::Text("Directional Light Settings");
			ImGui::Separator();
			ImGui::ColorEdit4("Light Color", &directionalLightData->color.x);
			ImGui::SliderFloat3("Light Direction", uiLightDirection, -1.0f, 1.0f);
			ImGui::SliderFloat("Light Intensity", &directionalLightData->intensity, 0.0f, 5.0f);
			ImGui::EndTabItem();
		}

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

		if (ImGui::BeginTabItem("Central Model")) {
			ImGui::Checkbox("Show Central Model", &showModel);
			if (showModel) {
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

		if (ImGui::BeginTabItem("Sphere")) {
			ImGui::Checkbox("Show Sphere", &showSphere);
			if (showSphere) {
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

		if (ImGui::BeginTabItem("Sprite")) {
			ImGui::Checkbox("Show Sprite", &showSprite);
			if (showSprite) {
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

		if (ImGui::BeginTabItem("Particle")) {
			ImGui::Checkbox("Show Particle", &showParticles);
			if (showParticles && particle_) {
				particle_->DrawImGui();
			}
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
	ImGui::End();
#endif
}

void GameScene::Draw() {
	ID3D12GraphicsCommandList* commandList = ctx_.dxCommon->GetCommandList();

	for (auto& model : modelEntries) {
		Model::PreDraw();
		model->Draw();
		Model::PostDraw();
	}

	if (showModel && planeModel && !planeModel->instances.empty()) {
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

	if (showSphere) {
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

	if (particle_ && showParticles) {
		particle_->Draw(commandList, textureSrvHandleCircle);
		commandList->SetGraphicsRootSignature(rootSignature_);
	}

	if (showSprite && planeModel && !planeModel->instances.empty()) {
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
}