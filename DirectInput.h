#pragma once
#include <Windows.h>
#include <Xinput.h>
#include "Common.h"

#pragma comment(lib, "xinput.lib")

class DirectInput
{
public:
	// コンストラクタ / デストラクタ
	DirectInput() = default;
	~DirectInput() = default;

	// 毎フレーム呼ぶ更新処理
	void Update();

	// ボタン入力判定
	// 押されているか
	bool IsPush(WORD button) const;
	// 押した瞬間か
	bool IsTrigger(WORD button) const;
	// 離した瞬間か
	bool IsRelease(WORD button) const;

	// --- スティック・トリガー入力 ---
	// 左スティック（x, y: -1.0f ~ 1.0f）
	Vector2 GetLeftStick() const;
	// 右スティック（x, y: -1.0f ~ 1.0f）
	Vector2 GetRightStick() const;
	// 左トリガー（L2 / LT: 0.0f ~ 1.0f）
	float GetLeftTrigger() const;
	// 右トリガー（R2 / RT: 0.0f ~ 1.0f）
	float GetRightTrigger() const;

	// 接続確認
	bool IsConnected() const { return isConnected_; }

private:
	XINPUT_STATE state_{};     // 現在のフレームの状態
	XINPUT_STATE statePre_{};  // 1フレーム前の状態
	bool isConnected_ = false; // パッドの接続フラグ
	DWORD userIndex_ = 0;      // プレイヤーインデックス（通常は0番）
};