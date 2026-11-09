#pragma once
#include "Engine/Base/Common.h"
#include "Engine/Base/SrvManager.h"
#include "Engine/Base/BaseScene.h"
#include "Engine/2d/ImGuiManager.h"

// ゲーム全体の骨組み(エンジン層)
// Draw が純粋仮想関数なので、継承しないとインスタンス生成できない
class Framework {
public:
	virtual ~Framework() = default;

	// 初期化
	virtual void Initialize();

	// 終了
	virtual void Finalize();

	// 毎フレーム更新
	virtual void Update();

	// 描画
	virtual void Draw() = 0;

	// 終了チェック
	virtual bool IsEndRequest() { return endRequest_; }

	// 実行(初期化 → ゲームループ → 終了)
	void Run();

protected:
	// 汎用データ(派生クラスから使う)
	std::unique_ptr<WinApp> winApp_;
	std::unique_ptr<DirectXCommon> dxCommon_;
	std::unique_ptr<SrvManager> srvManager_;
	std::unique_ptr<ImGuiManager> imguiManager_;

	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> dxcIncludeHandler_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// シーンへ渡すエンジン層の機能
	SceneContext sceneContext_;

private:
	void CreateRootSignature();

	bool endRequest_ = false;
};
