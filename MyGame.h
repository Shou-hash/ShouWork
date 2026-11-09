#pragma once
#include "Engine/Base/Framework.h"

// ゲーム固有の処理(アプリ層)。エンジン層の Framework を継承する
class MyGame : public Framework {
public:
	void Initialize() override;
	void Draw() override;
};
