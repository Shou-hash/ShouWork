#pragma once
#include <d3d12.h>

template <typename T>
class ResourceObject
{
private:
    T* resource_ = nullptr;

public:
    // デフォルトコンストラクタ
    ResourceObject() : resource_(nullptr) {}

    // ここでは AddRef() せずにそのまま所有権を保持します。
    ResourceObject(T* resource) : resource_(resource) {}

    // デストラクタでReleaseを呼ぶ
    ~ResourceObject() {
        if (resource_) {
            resource_->Release();
        }
    }

    // コピー禁止（安全のため、またはスマートポインタの厳密な管理のため）
    ResourceObject(const ResourceObject& other) = delete;
    ResourceObject& operator=(const ResourceObject& other) = delete;

    // ムーブコンストラクタ
    ResourceObject(ResourceObject&& other) noexcept : resource_(other.resource_) {
        other.resource_ = nullptr;
    }

    // ムーブ代入演算子
    ResourceObject& operator=(ResourceObject&& other) noexcept {
        if (this != &other) {
            if (resource_) {
                resource_->Release();
            }
            resource_ = other.resource_;
            other.resource_ = nullptr;
        }
        return *this;
    }

    // 生ポインタを明示的に取り出すGet関数
    T* Get() const { return resource_; }

    // CreateSwapChainForHwnd などの初期化関数に渡すための関数です。
    T** GetAddressOf() {
        if (resource_) {
            resource_->Release();
            resource_ = nullptr;
        }
        return &resource_;
    }

    // アロー演算子オーバーロード（COMオブジェクトのメンバ関数を直接呼ぶ用）
    T* operator->() const { return resource_; }

    // 有効なポインタを持っているかチェックする（boolキャスト）
    operator bool() const { return resource_ != nullptr; }
};