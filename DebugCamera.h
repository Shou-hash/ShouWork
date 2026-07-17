#pragma once
#include "Common.h"

/// <summary>
/// デバッグカメラ
/// </summary>
class DebugCamera
{
public:
    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize();

    /// <summary>
    /// 更新
    /// </summary>
    void Update();

    // --- ゲッターの追加 ---
    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

private:
    // ローカル座標
    Vector3 translation_ = { 0.0f, 0.0f, -50.0f };

    // 累積回転行列 (オイラー角管理から移行)
    Matrix4x4 matRot_;

    // ビュー行列
    Matrix4x4 viewMatrix_;

    // 射影行列
    Matrix4x4 projectionMatrix_;
};