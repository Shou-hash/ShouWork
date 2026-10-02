#include "DirectInput.h"
#include <cmath>

void DirectInput::Update()
{
	// 前フレームの状態を保存
	statePre_ = state_;

	// XInputの入力状態を取得
	DWORD result = XInputGetState(userIndex_, &state_);

	if (result == ERROR_SUCCESS)
	{
		isConnected_ = true;
	}
	else
	{
		isConnected_ = false;
		state_ = {}; // 切断時はクリア
	}
}

bool DirectInput::IsPush(WORD button) const
{
	if (!isConnected_) { return false; }
	return (state_.Gamepad.wButtons & button) != 0;
}

bool DirectInput::IsTrigger(WORD button) const
{
	if (!isConnected_) { return false; }
	return ((state_.Gamepad.wButtons & button) != 0) &&
		((statePre_.Gamepad.wButtons & button) == 0);
}

bool DirectInput::IsRelease(WORD button) const
{
	if (!isConnected_) { return false; }
	return ((state_.Gamepad.wButtons & button) == 0) &&
		((statePre_.Gamepad.wButtons & button) != 0);
}

Vector2 DirectInput::GetLeftStick() const
{
	if (!isConnected_) { return Vector2{ 0.0f, 0.0f }; }

	SHORT x = state_.Gamepad.sThumbLX;
	SHORT y = state_.Gamepad.sThumbLY;

	// デッドゾーン処理
	if (std::abs(x) < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
	{
		x = 0;
	}
	if (std::abs(y) < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
	{
		y = 0;
	}

	return Vector2{
		static_cast<float>(x) / 32767.0f,
		static_cast<float>(y) / 32767.0f
	};
}

Vector2 DirectInput::GetRightStick() const
{
	if (!isConnected_) { return Vector2{ 0.0f, 0.0f }; }

	SHORT x = state_.Gamepad.sThumbRX;
	SHORT y = state_.Gamepad.sThumbRY;

	// デッドゾーン処理
	if (std::abs(x) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) { x = 0; }
	if (std::abs(y) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) { y = 0; }

	return Vector2{
		static_cast<float>(x) / 32767.0f,
		static_cast<float>(y) / 32767.0f
	};
}

float DirectInput::GetLeftTrigger() const
{
	if (!isConnected_) { return 0.0f; }

	BYTE trigger = state_.Gamepad.bLeftTrigger;
	if (trigger < XINPUT_GAMEPAD_TRIGGER_THRESHOLD) { trigger = 0; }

	return static_cast<float>(trigger) / 255.0f;
}

float DirectInput::GetRightTrigger() const
{
	if (!isConnected_) { return 0.0f; }

	BYTE trigger = state_.Gamepad.bRightTrigger;
	if (trigger < XINPUT_GAMEPAD_TRIGGER_THRESHOLD) { trigger = 0; }

	return static_cast<float>(trigger) / 255.0f;
}