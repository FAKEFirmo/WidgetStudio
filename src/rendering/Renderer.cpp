#include "rendering/Renderer.h"

#include <algorithm>
#include <array>

namespace ws {
namespace {

D2D1_COLOR_F Color(float r, float g, float b, float a = 1.0f) {
    return D2D1::ColorF(r, g, b, a);
}

} // namespace

HRESULT Renderer::Initialize(HWND hwnd) {
    hwnd_ = hwnd;

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf());
    if (FAILED(hr)) return hr;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf()));
    if (FAILED(hr)) return hr;

    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(wicFactory_.GetAddressOf()));
    if (FAILED(hr)) return hr;

    hr = dwriteFactory_->CreateTextFormat(
        L"Segoe UI Variable",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        18.0f,
        L"en-us",
        labelFormat_.GetAddressOf());
    if (FAILED(hr)) {
        hr = dwriteFactory_->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            18.0f,
            L"en-us",
            labelFormat_.GetAddressOf());
    }
    if (FAILED(hr)) return hr;

    hr = dwriteFactory_->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        11.0f,
        L"en-us",
        smallFormat_.GetAddressOf());
    if (FAILED(hr)) return hr;

    labelFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    smallFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    hr = CreateDeviceResources();
    if (FAILED(hr)) return hr;

    // Wallpaper loading is optional. A solid fallback is rendered if Windows
    // does not expose a directly decodable wallpaper path (for example some
    // slideshow/theme configurations).
    ReloadWallpaper();
    return S_OK;
}

void Renderer::DiscardDeviceResources() noexcept {
    wallpaperBitmap_.Reset();
    renderTarget_.Reset();
}

HRESULT Renderer::CreateDeviceResources() {
    if (renderTarget_) return S_OK;

    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const D2D1_SIZE_U size = D2D1::SizeU(
        static_cast<UINT32>(std::max<LONG>(1, rc.right - rc.left)),
        static_cast<UINT32>(std::max<LONG>(1, rc.bottom - rc.top)));

    return d2dFactory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd_, size),
        renderTarget_.GetAddressOf());
}

HRESULT Renderer::Resize(UINT width, UINT height) {
    if (!renderTarget_) return S_OK;
    return renderTarget_->Resize(D2D1::SizeU(std::max(1u, width), std::max(1u, height)));
}

HRESULT Renderer::ReloadWallpaper() {
    wallpaperBitmap_.Reset();

    std::array<wchar_t, MAX_PATH> path{};
    if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, static_cast<UINT>(path.size()), path.data(), 0)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    if (path[0] == L'\0') {
        return S_FALSE;
    }

    return LoadBitmapFromFile(path.data());
}

HRESULT Renderer::LoadBitmapFromFile(const std::wstring& path) {
    if (!renderTarget_) {
        const HRESULT hr = CreateDeviceResources();
        if (FAILED(hr)) return hr;
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = wicFactory_->CreateDecoderFromFilename(
        path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        decoder.GetAddressOf());
    if (FAILED(hr)) return hr;

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) return hr;

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = wicFactory_->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(hr)) return hr;

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeMedianCut);
    if (FAILED(hr)) return hr;

    return renderTarget_->CreateBitmapFromWicBitmap(converter.Get(), nullptr, wallpaperBitmap_.GetAddressOf());
}

D2D1_RECT_F Renderer::ToD2D(RectF rect) const noexcept {
    return D2D1::RectF(rect.Left(), rect.Top(), rect.Right(), rect.Bottom());
}

void Renderer::DrawWallpaper() {
    const D2D1_SIZE_F targetSize = renderTarget_->GetSize();
    const D2D1_RECT_F destination = D2D1::RectF(0.0f, 0.0f, targetSize.width, targetSize.height);

    if (!wallpaperBitmap_) {
        renderTarget_->Clear(Color(0.92f, 0.91f, 0.89f));
        return;
    }

    const D2D1_SIZE_F bitmapSize = wallpaperBitmap_->GetSize();
    if (bitmapSize.width <= 0.0f || bitmapSize.height <= 0.0f) {
        renderTarget_->Clear(Color(0.92f, 0.91f, 0.89f));
        return;
    }

    const float targetAspect = targetSize.width / std::max(1.0f, targetSize.height);
    const float bitmapAspect = bitmapSize.width / std::max(1.0f, bitmapSize.height);

    D2D1_RECT_F source{};
    if (bitmapAspect > targetAspect) {
        const float sourceWidth = bitmapSize.height * targetAspect;
        const float left = (bitmapSize.width - sourceWidth) * 0.5f;
        source = D2D1::RectF(left, 0.0f, left + sourceWidth, bitmapSize.height);
    } else {
        const float sourceHeight = bitmapSize.width / targetAspect;
        const float top = (bitmapSize.height - sourceHeight) * 0.5f;
        source = D2D1::RectF(0.0f, top, bitmapSize.width, top + sourceHeight);
    }

    renderTarget_->DrawBitmap(
        wallpaperBitmap_.Get(),
        &destination,
        1.0f,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
        &source);
}

void Renderer::DrawGrid(const GridLayout& layout, const GridMetrics& metrics) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(renderTarget_->CreateSolidColorBrush(Color(0.08f, 0.08f, 0.09f, 0.14f), brush.GetAddressOf()))) {
        return;
    }

    for (int row = 0; row < layout.Rows(); ++row) {
        for (int column = 0; column < layout.Columns(); ++column) {
            const GridPlacement cell{column, row, 1, 1};
            const RectF rect = layout.RectFor(cell, metrics);
            const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(ToD2D(rect), 6.0f, 6.0f);
            renderTarget_->DrawRoundedRectangle(&rounded, brush.Get(), 1.0f);
        }
    }
}

void Renderer::DrawWidget(const WidgetInstance& widget, RectF rect, bool editMode) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> surfaceBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> borderBrush;

    if (FAILED(renderTarget_->CreateSolidColorBrush(
            Color(0.085f, 0.09f, 0.105f, widget.appearance.opacity),
            surfaceBrush.GetAddressOf()))) {
        return;
    }
    if (FAILED(renderTarget_->CreateSolidColorBrush(
            Color(1.0f, 1.0f, 1.0f, 0.16f),
            borderBrush.GetAddressOf()))) {
        return;
    }

    const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(
        ToD2D(rect),
        widget.appearance.cornerRadius,
        widget.appearance.cornerRadius);

    renderTarget_->FillRoundedRectangle(&rounded, surfaceBrush.Get());
    renderTarget_->DrawRoundedRectangle(&rounded, borderBrush.Get(), 1.0f);

    DrawWidgetPlaceholder(widget, rect);

    if (editMode && widget.selected) {
        DrawSelection(widget, rect);
    }
}

void Renderer::DrawWidgetPlaceholder(const WidgetInstance& widget, RectF rect) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> mutedBrush;
    if (FAILED(renderTarget_->CreateSolidColorBrush(Color(0.97f, 0.97f, 0.95f, 0.96f), textBrush.GetAddressOf()))) {
        return;
    }
    if (FAILED(renderTarget_->CreateSolidColorBrush(Color(0.97f, 0.97f, 0.95f, 0.52f), mutedBrush.GetAddressOf()))) {
        return;
    }

    const float padding = std::max(10.0f, std::min(rect.width, rect.height) * 0.09f);
    const std::wstring title = WidgetTypeName(widget.type);
    const std::wstring subtitle = L"Milestone 1 placeholder";

    const D2D1_RECT_F titleRect = D2D1::RectF(
        rect.Left() + padding,
        rect.Top() + padding,
        rect.Right() - padding,
        rect.Top() + padding + 28.0f);
    const D2D1_RECT_F subtitleRect = D2D1::RectF(
        rect.Left() + padding,
        rect.Top() + padding + 31.0f,
        rect.Right() - padding,
        rect.Top() + padding + 52.0f);

    renderTarget_->DrawTextW(
        title.c_str(),
        static_cast<UINT32>(title.size()),
        labelFormat_.Get(),
        &titleRect,
        textBrush.Get(),
        D2D1_DRAW_TEXT_OPTIONS_CLIP);

    renderTarget_->DrawTextW(
        subtitle.c_str(),
        static_cast<UINT32>(subtitle.size()),
        smallFormat_.Get(),
        &subtitleRect,
        mutedBrush.Get(),
        D2D1_DRAW_TEXT_OPTIONS_CLIP);

    // Simple widget-specific geometry makes it immediately obvious which
    // placeholder is which without implementing the production widget yet.
    if (widget.type == WidgetType::Music) {
        const float artSize = std::min(rect.height * 0.34f, rect.width * 0.18f);
        const RectF art{rect.Left() + padding, rect.Bottom() - padding - artSize, artSize, artSize};
        const D2D1_ROUNDED_RECT roundedArt = D2D1::RoundedRect(ToD2D(art), 10.0f, 10.0f);
        renderTarget_->FillRoundedRectangle(&roundedArt, mutedBrush.Get());
    } else if (widget.type == WidgetType::Photo) {
        const RectF imageRect{
            rect.Left() + padding,
            rect.Top() + padding + 58.0f,
            std::max(1.0f, rect.width - 2.0f * padding),
            std::max(1.0f, rect.height - 2.0f * padding - 58.0f)};
        const D2D1_ROUNDED_RECT roundedImage = D2D1::RoundedRect(ToD2D(imageRect), 10.0f, 10.0f);
        renderTarget_->DrawRoundedRectangle(&roundedImage, mutedBrush.Get(), 1.0f);
    }
}

void Renderer::DrawSelection(const WidgetInstance& widget, RectF rect) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    const float alpha = widget.primarySelection ? 0.96f : 0.70f;
    if (FAILED(renderTarget_->CreateSolidColorBrush(Color(1.0f, 1.0f, 1.0f, alpha), brush.GetAddressOf()))) {
        return;
    }

    const RectF expanded{rect.x - 3.0f, rect.y - 3.0f, rect.width + 6.0f, rect.height + 6.0f};
    const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(
        ToD2D(expanded),
        widget.appearance.cornerRadius + 3.0f,
        widget.appearance.cornerRadius + 3.0f);
    renderTarget_->DrawRoundedRectangle(
        &rounded,
        brush.Get(),
        widget.primarySelection ? 2.0f : 1.5f);
}

HRESULT Renderer::Render(
    const WidgetScene& scene,
    const GridLayout& layout,
    const GridMetrics& metrics,
    bool editMode) {

    HRESULT hr = CreateDeviceResources();
    if (FAILED(hr)) return hr;

    renderTarget_->BeginDraw();
    DrawWallpaper();

    if (editMode) {
        DrawGrid(layout, metrics);
    }

    for (const auto& widget : scene.Widgets()) {
        DrawWidget(widget, layout.RectFor(widget.grid, metrics), editMode);
    }

    hr = renderTarget_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        hr = CreateDeviceResources();
        if (SUCCEEDED(hr)) {
            ReloadWallpaper();
        }
    }
    return hr;
}

} // namespace ws
