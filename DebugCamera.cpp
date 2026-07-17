#include "DebugCamera.h"
#include <cmath>

// 必要に応じてキー入力を外部から取得するための仕組み、
// あるいは WinApp 等から状態を参照できるようにする設定をここに加えます。
// ここでは、main.cpp 同様に DIK_W, DIK_S, DIK_A, DIK_D や矢印キー等による操作を想定します。
extern BYTE key[256]; 

void DebugCamera::Initialize()
{
    // 各種メンバ変数の初期化
    translation_ = { 0.0f, 0.0f, -5.0f }; // 初期位置（main.cppのcameraTransform.translateに合わせる場合は -5.0f など）
    matRot_ = MakeIdentity4x4();          // 回転行列を単位行列で初期化
    viewMatrix_ = MakeIdentity4x4();
    
    // 射影行列の初期設定 (1280x720 想定)
    projectionMatrix_ = MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 100.0f);
}

void DebugCamera::Update()
{
    // 1. 入力によるカメラの移動や回転
    
    // --- 回転処理 ---
    // 累積回転行列に対して、毎フレームの入力分の回転を掛け合わせます
    Matrix4x4 matFrameRot = MakeIdentity4x4();
    float rotateSpeed = 0.02f; // 回転の速さ
    
    // 例：テンキーや矢印キー、マウス入力などで回転
    if (key[DIK_UP]) {
        // X軸周り（ピッチ）の回転行列を作成して乗算
        matFrameRot = Multiply(matFrameRot, MakeRotateXMatrix(-rotateSpeed));
    }
    if (key[DIK_DOWN]) {
        matFrameRot = Multiply(matFrameRot, MakeRotateXMatrix(rotateSpeed));
    }
    if (key[DIK_LEFT]) {
        // Y軸周り（ヨー）の回転行列を作成して乗算
        matFrameRot = Multiply(matFrameRot, MakeRotateYMatrix(-rotateSpeed));
    }
    if (key[DIK_RIGHT]) {
        matFrameRot = Multiply(matFrameRot, MakeRotateYMatrix(rotateSpeed));
    }
    // 累積回転行列に適用
    matRot_ = Multiply(matRot_, matFrameRot);


    // --- 移動処理 ---
    Vector3 move = { 0.0f, 0.0f, 0.0f };
    float speed = 0.2f; // 移動の速さ

    // 前後移動 (W, S)
    if (key[DIK_W]) {
        move.z += speed;
    }
    if (key[DIK_S]) {
        move.z -= speed;
    }
    // 左右移動 (A, D)
    if (key[DIK_A]) {
        move.x -= speed;
    }
    if (key[DIK_D]) {
        move.x += speed;
    }
    // 上下移動 (Space, Shiftキーなど)
    if (key[DIK_SPACE]) {
        move.y += speed;
    }
    if (key[DIK_LSHIFT]) {
        move.y -= speed;
    }

    // 移動ベクトルを現在の回転行列分だけ回転させる
    Vector3 rotatedMove = TransformNormal(move, matRot_);
    
    // 座標を加算する
    translation_.x += rotatedMove.x;
    translation_.y += rotatedMove.y;
    translation_.z += rotatedMove.z;


    // 2. ビュー行列の更新 (レールカメラや通常のカメラと同じ処理)
    
    // 座標から平行移動行列を計算する
    Matrix4x4 matTrans = MakeTranslateMatrix(translation_);
    
    // 回転行列と平行移動行列からワールド行列を計算する
    Matrix4x4 worldMatrix = Multiply(matRot_, matTrans);
    
    // ワールド行列の逆行列をビュー行列に代入する
    viewMatrix_ = Inverse(worldMatrix);
}