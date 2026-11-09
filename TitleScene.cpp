#include "TitleScene.h"
#include "GameScene.h"
#include "Engine/Base/SceneManager.h"
#include "Engine/2d/ImGuiManager.h"

// タイトル画像(差し替える場合はここを変更)
static const char* kTitleTexturePath = "Resources/monsterBall.png";

void TitleScene::Initialize()
{
	ID3D12Device* device = ctx_.dxCommon->GetDevice();

	input_ = std::make_unique<Input>();
	input_->Initialize(ctx_.winApp->GetHInstance(), ctx_.winApp->GetHwnd());

	// タイトル画像の読み込み
	textureHandle_ = TextureManager::GetInstance()->LoadTextureAndCreateSRV(kTitleTexturePath);

	// 板ポリ(GameScene のスプライトと同じ作り方)
	ModelLoader::Initialize(device, ctx_.dxCommon->GetCommandList(), textureHandle_);
	planeModel_ = Model::CreateFromOBJ("plane", false);

	// マテリアル(ライティングなし)
	materialResource_ = CreateBufferResource(device, sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = 0;
	materialData_->uvTransform = MakeIdentity4x4();

	// ルートシグネチャが要求するので平行光源も用意する
	lightResource_ = CreateBufferResource(device, sizeof(DirectionalLight));
	lightResource_->Map(0, nullptr, reinterpret_cast<void**>(&lightData_));
	lightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	lightData_->direction = { 0.0f, -1.0f, 0.0f };
	lightData_->intensity = 1.0f;

	// 画面中央に表示する2D用WVP(位置・大きさは見た目に合わせて調整)
	wvpResource_ = CreateBufferResource(device, sizeof(TransformationMatrix));
	wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
	Matrix4x4 world = MakeAffineMatrix({ 320.0f, 180.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 640.0f, 360.0f, 0.0f });
	Matrix4x4 projection = MakeOrthographicMatrix(0.0f, 1280.0f, 0.0f, 720.0f, 0.0f, 100.0f);
	wvpData_->WVP = Multiply(world, projection);
	wvpData_->World = world;
}

void TitleScene::Finalize()
{
	input_.reset();
}

void TitleScene::Update()
{
	input_->Update();

#ifdef USE_IMGUI
	ImGui::Begin("Title");
	ImGui::Text("Press SPACE to start");
	ImGui::End();
#endif

	// SPACEでゲームシーンへ(予約するだけ。切り替えはシーンマネージャが次フレームで行う)
	if (input_->TriggerKey(DIK_SPACE)) {
		SceneManager::GetInstance()->SetNextScene(std::make_unique<GameScene>(ctx_));
	}
}

void TitleScene::Draw()
{
	ID3D12GraphicsCommandList* commandList = ctx_.dxCommon->GetCommandList();
	BlendManager::GetInstance()->SetPipelineState(commandList, kBlendModeNormal);

	auto& inst = planeModel_->instances[0];
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, lightResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(2, wvpResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(3, textureHandle_);
	commandList->IASetVertexBuffers(0, 1, &inst.vertexBufferView);
	commandList->DrawInstanced(inst.vertexCount, 1, 0, 0);
}
