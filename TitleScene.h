#pragma once
#include "Engine/Base/BaseScene.h"

// タイトルシーン(アプリ層)
class TitleScene : public BaseScene {
public:
	using BaseScene::BaseScene; // コンストラクタを継承

	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	std::unique_ptr<Input> input_;
	std::shared_ptr<Model> planeModel_; // スプライト用の板ポリ

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
	DirectionalLight* lightData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
	TransformationMatrix* wvpData_ = nullptr;

	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_{};
};
