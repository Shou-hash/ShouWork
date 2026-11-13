#pragma once
#include "Engine/Base/Common.h"
#include "Engine/Base/SrvManager.h"

// シーンが使うエンジン層の機能(Frameworkが所有するものを借りる)
struct SceneContext {
	WinApp* winApp = nullptr;
	DirectXCommon* dxCommon = nullptr;
	SrvManager* srvManager = nullptr;
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	IDxcIncludeHandler* dxcIncludeHandler = nullptr;
	ID3D12RootSignature* rootSignature = nullptr;
};

// シーン基底クラス(エンジン層)。具体的なシーン(アプリ層)はこれを継承する
class BaseScene {
public:
	explicit BaseScene(const SceneContext& context) : ctx_(context) {}
	virtual ~BaseScene() = default;

	// 初期化
	virtual void Initialize() = 0;

	// 終了
	virtual void Finalize() = 0;

	// 毎フレーム更新
	virtual void Update() = 0;

	// 描画
	virtual void Draw() = 0;

protected:
	SceneContext ctx_;
};
