#pragma once
#include <d3d12.h>
#include "Engine/Base/Common.h"

class Model;
class DebugCamera;

class ModelDraw {
public:
    /// <summary>
    /// 初期化（コマンドリストおよび光源リソースの登録）
    /// </summary>
    static void Initialize(ID3D12GraphicsCommandList* commandList, ID3D12Resource* directionalLightResource);

    /// <summary>
    /// 描画前共通処理
    /// </summary>
    static void PreDraw();

    /// <summary>
    /// 描画後共通処理
    /// </summary>
    static void PostDraw();

    /// <summary>
    /// コマンドリストの更新（フレーム毎に変更される場合）
    /// </summary>
    static void SetCommandList(ID3D12GraphicsCommandList* commandList);

    /// <summary>
    /// 平行光源リソースの更新
    /// </summary>
    static void SetDirectionalLightResource(ID3D12Resource* lightResource);

    /// <summary>
    /// モデルの描画（Transform と Camera を指定して行列を自動計算して描画）
    /// </summary>
    static void Draw(Model* model, const struct Transform& transform, const DebugCamera& camera, D3D12_GPU_DESCRIPTOR_HANDLE overrideTexHandle = { 0 });

    /// <summary>
    /// モデルの描画（事前に計算済みの WVP 行列を使用して描画）
    /// </summary>
    static void Draw(Model* model, D3D12_GPU_DESCRIPTOR_HANDLE overrideTexHandle = { 0 });

private:
    static ID3D12GraphicsCommandList* sCommandList_;
    static ID3D12Resource* sDirectionalLightResource_;
};