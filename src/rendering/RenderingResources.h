#pragma once

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

namespace ws {

// Process-owned immutable factories and common text formats. Every widget HWND
// keeps its own device-dependent render target, while these expensive factory
// objects are created once and shared on the UI thread.
class RenderingResources {
public:
    HRESULT Initialize();

    [[nodiscard]] ID2D1Factory* D2DFactory() const noexcept { return d2dFactory_.Get(); }
    [[nodiscard]] IDWriteFactory* DWriteFactory() const noexcept { return dwriteFactory_.Get(); }
    [[nodiscard]] IWICImagingFactory* WicFactory() const noexcept { return wicFactory_.Get(); }
    [[nodiscard]] IDWriteTextFormat* LabelFormat() const noexcept { return labelFormat_.Get(); }
    [[nodiscard]] IDWriteTextFormat* SmallFormat() const noexcept { return smallFormat_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> labelFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> smallFormat_;
};

} // namespace ws
