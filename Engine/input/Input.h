#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl/client.h>
#include <cstdint>
#include <cassert>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input {
public:
    Input() = default;
    ~Input() = default;

    // 初期化
    void Initialize(HINSTANCE hInstance, HWND hwnd);

    // 毎フレームの更新処理
    void Update();

    // キー押下判定
    bool PushKey(uint8_t keyNumber) const;

    // キーのトリガー（押した瞬間）判定
    bool TriggerKey(uint8_t keyNumber) const;

    // キーバッファの取得（DebugCamera等で利用する場合）
    const BYTE* GetKeyBuffer() const { return key_; }

private:
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;

    BYTE key_[256] = {};
    BYTE keyPre_[256] = {};
};