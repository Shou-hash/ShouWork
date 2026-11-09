#include "MyGame.h"
#include "SceneFactory.h"
#include "Engine/Base/SceneManager.h"

void MyGame::Initialize()
{
	// 基底クラスの初期化処理
	Framework::Initialize();

	// シーン工場を生成し、マネージャにセット
	sceneFactory_ = std::make_unique<SceneFactory>(sceneContext_);
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());

	// 最初のシーンをシーン名で指定(初期化はマネージャ内で呼ばれる)
	SceneManager::GetInstance()->ChangeScene("TITLE");
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
