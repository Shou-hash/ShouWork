#pragma once
#include "Engine/Base/BaseScene.h"
#include <string>

// シーン工場(概念)。ゲーム固有の具体的な工場は、これを継承して作る
class AbstractSceneFactory {
public:
	// 仮想デストラクタ
	virtual ~AbstractSceneFactory() = default;

	// シーン生成(指定した名前のシーンがなければ nullptr)
	virtual std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) = 0;
};
