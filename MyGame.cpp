#include "MyGame.h"
#include "TitleScene.h"
#include "Engine/Base/SceneManager.h"

void MyGame::Initialize()
{
	// 基底クラスの初期化処理
	Framework::Initialize();

	// 最初のシーンをシーンマネージャにセット(初期化はマネージャ内で呼ばれる)
	SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>(sceneContext_));
}

void MyGame::Draw()
{
	// 描画前処理
	dxCommon_->PreDraw();
	srvManager_->PreDraw();
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());

	// シーンの描画
	SceneManager::GetInstance()->Draw();

	// ImGui描画(全Drawの最後、PostDrawの直前)
	imguiManager_->Draw();

	// 描画後処理(60FPS固定)
	dxCommon_->PostDraw();
}
