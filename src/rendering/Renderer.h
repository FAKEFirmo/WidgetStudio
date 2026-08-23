#pragma once

#include "common/Geometry.h"
#include "layout/GridLayout.h"
#include "scene/WidgetScene.h"

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <windows.h>

#include <string>
#include <cstdint>

namespace ws {

class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;

    HRESULT Initialize(HWND hwnd);
    void DiscardDeviceResources() noexcept;
    HRESULT Resize(UINT width, UINT height);
    void SetDpi(float dpi) noexcept;

    HRESULT ReloadWallpaper();

    HRESULT Render(
        const WidgetScene& scene,
        const GridLayout& layout,
        const GridMetrics& metrics,
        bool editMode);

private:
    HRESULT CreateDeviceResources();
    HRESULT LoadBitmapFromFile(const std::wstring& path);

    void DrawWallpaper();
    void DrawGrid(const GridLayout& layout, const GridMetrics& metrics);
    void DrawWidget(const WidgetInstance& widget, RectF rect, bool editMode);
    void DrawSelection(const WidgetInstance& widget, RectF rect);

    D2D1_RECT_F ToD2D(RectF rect) const noexcept;

    HWND hwnd_{};
    float dpi_{96.0f};

    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> renderTarget_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> wallpaperBitmap_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> labelFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> smallFormat_;
    std::uint64_t resourceGeneration_{};
};

} // namespace ws
