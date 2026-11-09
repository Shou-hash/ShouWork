#pragma once
#include "Engine/Base/BaseScene.h"

// シーン管理(エンジン層・Singleton)
// 実行中シーンの所有・切り替え・更新・描画を担当する
class SceneManager {
public:
	static SceneManager* GetInstance();

	// 切り替え処理 + 実行中シーンの更新
	void Update();

	// 実行中シーンの描画
	void Draw();

	// 終了処理(実行中シーンの終了と解放)
	void Finalize();

	// 次シーン予約(切り替えは次のUpdateで行われる)
	void SetNextScene(std::unique_ptr<BaseScene> nextScene) { nextScene_ = std::move(nextScene); }

private:
	SceneManager() = default;
	~SceneManager() = default;
	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

private:
	// 今のシーン(実行中シーン)
	std::unique_ptr<BaseScene> scene_;
	// 次のシーン
	std::unique_ptr<BaseScene> nextScene_;
};
