#pragma once
#include "Engine/Base/AbstractSceneFactory.h"

// このゲーム用のシーン工場
class SceneFactory : public AbstractSceneFactory {
public:
	explicit SceneFactory(const SceneContext& context) : ctx_(context) {}

	// シーン生成
	std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) override;

private:
	SceneContext ctx_;
};
