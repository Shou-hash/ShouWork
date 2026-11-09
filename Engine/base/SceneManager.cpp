#include "SceneManager.h"

SceneManager* SceneManager::GetInstance()
{
	static SceneManager instance;
	return &instance;
}

void SceneManager::Update()
{
	// 次シーンの予約があるなら切り替える
	if (nextScene_) {
		// 旧シーンの終了
		if (scene_) {
			scene_->Finalize();
		}

		// シーン切り替え(旧シーンはここで解放される)
		scene_ = std::move(nextScene_);

		// 新シーンの初期化
		scene_->Initialize();
	}

	// 実行中シーンの更新
	if (scene_) {
		scene_->Update();
	}
}

void SceneManager::Draw()
{
	if (scene_) {
		scene_->Draw();
	}
}

void SceneManager::Finalize()
{
	// 最後のシーンの終了と解放
	if (scene_) {
		scene_->Finalize();
		scene_.reset();
	}
	nextScene_.reset();
}
