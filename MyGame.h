#pragma once
#include "Engine/Base/Framework.h"
#include "GameScene.h"

// ゲーム固有の処理(アプリ層)。エンジン層の Framework を継承する
class MyGame : public Framework {
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	std::unique_ptr<GameScene> gameScene_;
};
