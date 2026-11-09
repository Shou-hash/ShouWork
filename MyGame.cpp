#include "MyGame.h"

void MyGame::Initialize()
{
	// 基底クラスの初期化処理
	Framework::Initialize();

	// ゲーム固有の初期化
	gameScene_ = std::make_unique<GameScene>();
	gameScene_->Initialize(
		dxCommon_->GetDevice(),
		dxCommon_->GetCommandList(),
		winApp_.get(),
		srvManager_.get(),
		dxcUtils_.Get(),
		dxcCompiler_.Get(),
		dxcIncludeHandler_.Get(),
		rootSignature_.Get()
	);
}

void MyGame::Finalize()
{
	// ゲーム固有の終了処理
	gameScene_.reset();

	// 基底クラスの終了処理
	Framework::Finalize();
}

void MyGame::Update()
{
	// 基底クラスの更新処理
	Framework::Update();

	// ゲーム固有の更新処理(ImGuiの表示項目はBegin〜Endの間で追加される)
	imguiManager_->Begin();
	gameScene_->Update();
	imguiManager_->End();
}

void MyGame::Draw()
{
	// 描画前処理
	dxCommon_->PreDraw();
	srvManager_->PreDraw();
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());

	// ゲームシーンの描画
	gameScene_->Draw(dxCommon_->GetCommandList());

	// ImGui描画(全Drawの最後、PostDrawの直前)
	imguiManager_->Draw();

	// 描画後処理(60FPS固定)
	dxCommon_->PostDraw();
}
