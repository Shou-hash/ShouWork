#include "Input.h"
#include <cstring>

void Input::Initialize(HINSTANCE hInstance, HWND hwnd) {
    HRESULT hr = DirectInput8Create(
        hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
        reinterpret_cast<void**>(directInput_.GetAddressOf()), nullptr);
    assert(SUCCEEDED(hr));

    hr = directInput_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), NULL);
    assert(SUCCEEDED(hr));

    hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
    assert(SUCCEEDED(hr));

    hr = keyboard_->SetCooperativeLevel(
        hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(hr));
}

void Input::Update() {
    // 前フレームのキー状態を保存
    std::memcpy(keyPre_, key_, sizeof(key_));

    // キーボードの入力状態を取得
    keyboard_->Acquire();
    keyboard_->GetDeviceState(sizeof(key_), key_);
}

bool Input::PushKey(uint8_t keyNumber) const {
    return key_[keyNumber] != 0;
}

bool Input::TriggerKey(uint8_t keyNumber) const {
    return (key_[keyNumber] != 0) && (keyPre_[keyNumber] == 0);
}