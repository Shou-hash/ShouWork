#include "DebugCamera.h"
#include <cmath>

// main.cppで定義されているDirectInputのキー入力バッファを参照
extern BYTE key[256];

void DebugCamera::Initialize()
{
    // メンバ変数の初期化
    rotation_ = { 0.0f, 0.0f, 0.0f };
    translation_ = { 0.0f, 0.0f, -20.0f }; // モデルが見やすい距離に設定
    viewMatrix_ = MakeIdentity4x4();

    // 射影行列の生成 (アスペクト比 1280x720)
    projectionMatrix_ = MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 100.0f);
}

void DebugCamera::Update()
{
    // ==========================================
    // 1. 入力によるカメラの移動や回転
    // ==========================================

    // --- 回転処理 ---
    float rotateSpeed = 0.02f;

    // X軸周り（上下回転）
    if (key[DIK_UP]) {
        rotation_.x -= rotateSpeed;
    }
    if (key[DIK_DOWN]) {
        rotation_.x += rotateSpeed;
    }
    // Y軸周り（左右回転）
    if (key[DIK_LEFT]) {
        rotation_.y -= rotateSpeed;
    }
    if (key[DIK_RIGHT]) {
        rotation_.y += rotateSpeed;
    }

    // --- 移動処理 ---
    float moveSpeed = 0.2f;
    Vector3 move = { 0.0f, 0.0f, 0.0f };

    // 前後移動 (W, S)
    if (key[DIK_W]) { move.z += moveSpeed; }
    if (key[DIK_S]) { move.z -= moveSpeed; }

    // 左右移動 (A, D)
    if (key[DIK_D]) { move.x += moveSpeed; }
    if (key[DIK_A]) { move.x -= moveSpeed; }

    // 上下移動 (Space, Left Shift)
    if (key[DIK_SPACE]) { move.y += moveSpeed; }
    if (key[DIK_LSHIFT]) { move.y -= moveSpeed; }

    // 角度から回転行列を計算 (Y -> X -> Z の順などで合成)
    Matrix4x4 matRotX = MakeRotateXMatrix(rotation_.x);
    Matrix4x4 matRotY = MakeRotateYMatrix(rotation_.y);
    Matrix4x4 matRotZ = MakeRotateZMatrix(rotation_.z);
    Matrix4x4 matRot = Multiply(matRotX, Multiply(matRotY, matRotZ));

    // 移動ベクトルをカメラの回転分だけ回転させる
    Vector3 rotatedMove = TransformNormal(move, matRot);

    // 移動ベクトル分だけ座標を加算する
    translation_.x += rotatedMove.x;
    translation_.y += rotatedMove.y;
    translation_.z += rotatedMove.z;

    // ==========================================
    // 2. ビュー行列の更新 (資料5枚目の手順)
    // ==========================================

    // 座標から平行移動行列を計算する
    Matrix4x4 matTrans = MakeTranslateMatrix(translation_);

    // 回転行列と平行移動行列からワールド行列を計算する
    Matrix4x4 worldMatrix = Multiply(matRot, matTrans);

    // ワールド行列の逆行列をビュー行列に代入する
    viewMatrix_ = Inverse(worldMatrix);
}