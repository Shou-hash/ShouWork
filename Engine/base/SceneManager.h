#pragma once
#include "Engine/Base/BaseScene.h"
#include "Engine/Base/AbstractSceneFactory.h"
#include <string>

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

	// 次シーン予約(シーン名を渡す。切り替えは次のUpdateで行われる)
	void ChangeScene(const std::string& sceneName);

	// シーン工場のsetter(借りるだけ。所有・解放はFramework)
	void SetSceneFactory(AbstractSceneFactory* sceneFactory) { sceneFactory_ = sceneFactory; }

private:
	SceneManager() = default;
	~SceneManager() = default;
	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

private:
	// シーン工場(借りてくる)
	AbstractSceneFactory* sceneFactory_ = nullptr;

	// 今のシーン(実行中シーン)
	std::unique_ptr<BaseScene> scene_;
	// 次のシーン
	std::unique_ptr<BaseScene> nextScene_;
};
