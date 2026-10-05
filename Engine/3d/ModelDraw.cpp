#include "ModelDraw.h"
#include "ModelLoader.h"
#include "Engine/3d/DebugCamera.h"
#include "Engine/Base/BlendManager.h"
#include <cassert>

ID3D12GraphicsCommandList* ModelDraw::sCommandList_ = nullptr;
ID3D12Resource* ModelDraw::sDirectionalLightResource_ = nullptr;

void ModelDraw::Initialize(ID3D12GraphicsCommandList* commandList, ID3D12Resource* directionalLightResource) {
	sCommandList_ = commandList;
	sDirectionalLightResource_ = directionalLightResource;
}

void ModelDraw::SetCommandList(ID3D12GraphicsCommandList* commandList) {
	sCommandList_ = commandList;
}

void ModelDraw::SetDirectionalLightResource(ID3D12Resource* lightResource) {
	sDirectionalLightResource_ = lightResource;
}

void ModelDraw::PreDraw() {
	assert(sCommandList_ && "ModelDraw::Initialize が呼び出されていないか、sCommandList_ が nullptr です。");
}

void ModelDraw::PostDraw() {
	// 描画後の処理（必要に応じて記述）
}

void ModelDraw::Draw(Model* model, const struct Transform& transform, const DebugCamera& camera, D3D12_GPU_DESCRIPTOR_HANDLE overrideTexHandle) {
	if (!model) { return; }
	assert(sCommandList_ && "ModelDraw::sCommandList_ が設定されていません。");

	// Transform と Camera から行列を更新計算
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 viewMatrix = camera.GetViewMatrix();
	Matrix4x4 projectionMatrix = camera.GetProjectionMatrix();
	Matrix4x4 wvpMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));

	for (auto& inst : model->instances) {
		if (!inst.visible || inst.vertexCount == 0) { continue; }

		// ★【追加】インスタンスのブレンドモードを設定
		BlendManager::GetInstance()->SetPipelineState(sCommandList_, inst.blendMode);

		// 行列バッファの更新
		if (inst.wvpData) {
			inst.wvpData->WVP = wvpMatrix;
			inst.wvpData->World = worldMatrix;
		}

		// テクスチャハンドルの決定
		D3D12_GPU_DESCRIPTOR_HANDLE drawTexHandle = inst.defaultSrvGpuHandle;
		if (overrideTexHandle.ptr != 0) {
			drawTexHandle = overrideTexHandle;
		}

		// 定数バッファおよびディスクリプタテーブルの設定
		sCommandList_->SetGraphicsRootConstantBufferView(0, inst.materialResource->GetGPUVirtualAddress());
		if (sDirectionalLightResource_) {
			sCommandList_->SetGraphicsRootConstantBufferView(1, sDirectionalLightResource_->GetGPUVirtualAddress());
		}
		sCommandList_->SetGraphicsRootConstantBufferView(2, inst.wvpResource->GetGPUVirtualAddress());
		sCommandList_->SetGraphicsRootDescriptorTable(3, drawTexHandle);
		sCommandList_->IASetVertexBuffers(0, 1, &inst.vertexBufferView);
		sCommandList_->DrawInstanced(inst.vertexCount, 1, 0, 0);
	}
}

void ModelDraw::Draw(Model* model, D3D12_GPU_DESCRIPTOR_HANDLE overrideTexHandle) {
	if (!model) return;
	assert(sCommandList_ && "ModelDraw::sCommandList_ が設定されていません。");

	for (auto& inst : model->instances) {
		if (!inst.visible || inst.vertexCount == 0) { continue; }

		// ★【追加】インスタンスのブレンドモードを設定
		BlendManager::GetInstance()->SetPipelineState(sCommandList_, inst.blendMode);

		D3D12_GPU_DESCRIPTOR_HANDLE drawTexHandle = inst.defaultSrvGpuHandle;
		if (overrideTexHandle.ptr != 0) {
			drawTexHandle = overrideTexHandle;
		}

		sCommandList_->SetGraphicsRootConstantBufferView(0, inst.materialResource->GetGPUVirtualAddress());
		if (sDirectionalLightResource_) {
			sCommandList_->SetGraphicsRootConstantBufferView(1, sDirectionalLightResource_->GetGPUVirtualAddress());
		}
		sCommandList_->SetGraphicsRootConstantBufferView(2, inst.wvpResource->GetGPUVirtualAddress());
		sCommandList_->SetGraphicsRootDescriptorTable(3, drawTexHandle);
		sCommandList_->IASetVertexBuffers(0, 1, &inst.vertexBufferView);
		sCommandList_->DrawInstanced(inst.vertexCount, 1, 0, 0);
	}
}