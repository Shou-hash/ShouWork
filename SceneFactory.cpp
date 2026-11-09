#include "SceneFactory.h"
#include "TitleScene.h"
#include "GameScene.h"

std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName)
{
	// 指定された文字列と一致するシーンを生成して返す
	if (sceneName == "TITLE") {
		return std::make_unique<TitleScene>(ctx_);
	}
	if (sceneName == "GAMEPLAY") {
		return std::make_unique<GameScene>(ctx_);
	}

	// 一致しなければ nullptr
	return nullptr;
}
