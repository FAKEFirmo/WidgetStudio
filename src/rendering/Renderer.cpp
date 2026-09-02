#include "rendering/Renderer.h"
#include "layout/OuterLayout.h"
#include "rendering/RenderingResources.h"
#include "rendering/WidgetRenderContext.h"
#include "rendering/WidgetVisualStyle.h"
#include "rendering/WallpaperCache.h"

#include <algorithm>
#include <array>
#include <cwchar>
#include <utility>

namespace ws {
namespace {

D2D1_COLOR_F Color(float r, float g, float b, float a = 1.0f) {
    return D2D1::ColorF(r, g, b, a);
}

bool SameRect(RectF left, RectF right) noexcept {
    return left.x == right.x && left.y == right.y &&
        left.width == right.width && left.height == right.height;
}

} // namespace

Renderer::Renderer(std::shared_ptr<WallpaperCache> wallpaperCache,
    std::shared_ptr<RenderingResources> resources)
    : resources_(std::move(resources)), wallpaperCache_(std::move(wallpaperCache)) {}

void Renderer::SetWallpaperCache(std::shared_ptr<WallpaperCache> wallpaperCache) {
    if (!wallpaperCache || wallpaperCache_ == wallpaperCache) return;
    wallpaperCache_ = std::move(wallpaperCache);
    wallpaperBitmap_.Reset();
    glassCache_.clear();
    wallpaperRevision_ = 0;
}

void Renderer::SetSharedResources(std::shared_ptr<RenderingResources> resources) {
    if (!resources || resources_ == resources) return;
    DiscardDeviceResources();
    resources_ = std::move(resources);
}

HRESULT Renderer::Initialize(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return E_INVALIDARG;
    hwnd_ = hwnd;
    if (!wallpaperCache_) wallpaperCache_ = std::make_shared<WallpaperCache>();
    if (!resources_) resources_ = std::make_shared<RenderingResources>();
    dpi_ = static_cast<float>(GetDpiForWindow(hwnd_));
    if (dpi_ <= 0.0f) {
        dpi_ = 96.0f;
    }

    HRESULT hr = resources_->Initialize();
    if (FAILED(hr)) return hr;

    hr = CreateDeviceResources();
    if (FAILED(hr)) return hr;

    // Wallpaper loading is optional. A solid fallback is rendered if Windows
    // does not expose a directly decodable wallpaper path.
    static_cast<void>(wallpaperCache_->Initialize());
    return S_OK;
}

void Renderer::DiscardDeviceResources() noexcept {
    glassCache_.clear();
    wallpaperBitmap_.Reset();
    renderTarget_.Reset();
}

HRESULT Renderer::CreateDeviceResources() {
    if (renderTarget_) return S_OK;

    if (!hwnd_ || !IsWindow(hwnd_)) return HRESULT_FROM_WIN32(ERROR_INVALID_WINDOW_HANDLE);
    RECT rc{};
    if (!GetClientRect(hwnd_, &rc)) return HRESULT_FROM_WIN32(GetLastError());
    const D2D1_SIZE_U size = D2D1::SizeU(
        static_cast<UINT32>(std::max<LONG>(1, rc.right - rc.left)),
        static_cast<UINT32>(std::max<LONG>(1, rc.bottom - rc.top)));

    const HRESULT hr = resources_->D2DFactory()->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE),
        D2D1::HwndRenderTargetProperties(hwnd_, size),
        renderTarget_.GetAddressOf());
    if (SUCCEEDED(hr)) {
        renderTarget_->SetDpi(dpi_, dpi_);
        ++resourceGeneration_;
    }
    return hr;
}

HRESULT Renderer::Resize(UINT width, UINT height) {
    if (!renderTarget_) return S_OK;
    glassCache_.clear();
    wallpaperBitmap_.Reset();
    const HRESULT result = renderTarget_->Resize(
        D2D1::SizeU(std::max(1u, width), std::max(1u, height)));
    if (result == D2DERR_RECREATE_TARGET) {
        // The next paint recreates the target against the HWND's current
        // client size. Keeping a rejected target here causes repeated errors.
        DiscardDeviceResources();
        return S_OK;
    }
    return result;
}

void Renderer::SetDpi(float dpi) noexcept {
    dpi_ = dpi > 0.0f ? dpi : 96.0f;
    if (renderTarget_) {
        renderTarget_->SetDpi(dpi_, dpi_);
    }
    glassCache_.clear();
    wallpaperBitmap_.Reset();
}

HRESULT Renderer::ReloadWallpaper() {
    wallpaperBitmap_.Reset();
    glassCache_.clear();
    return wallpaperCache_->Reload();
}

HRESULT Renderer::EnsureWallpaperBitmap() {
    if (!wallpaperCache_ || wallpaperMonitor_.monitorWidth <= 0 ||
        wallpaperMonitor_.monitorHeight <= 0) return S_FALSE;
    if (wallpaperRevision_ != wallpaperCache_->Revision()) {
        wallpaperBitmap_.Reset();
        glassCache_.clear();
        wallpaperRevision_ = wallpaperCache_->Revision();
    }
    if (wallpaperBitmap_) return S_OK;

    Microsoft::WRL::ComPtr<IWICBitmapSource> monitorBitmap;
    HRESULT result = wallpaperCache_->MonitorBitmap(
        wallpaperMonitorId_, wallpaperMonitor_, monitorBitmap.GetAddressOf());
    if (result != S_OK) return result;
    const int left = std::clamp(static_cast<int>(std::floor(wallpaperWindowPixels_.Left())) -
            wallpaperSamplingPaddingPixels_,
        0, wallpaperMonitor_.monitorWidth - 1);
    const int top = std::clamp(static_cast<int>(std::floor(wallpaperWindowPixels_.Top())) -
            wallpaperSamplingPaddingPixels_,
        0, wallpaperMonitor_.monitorHeight - 1);
    const int right = std::clamp(static_cast<int>(std::ceil(wallpaperWindowPixels_.Right())) +
            wallpaperSamplingPaddingPixels_,
        left + 1, wallpaperMonitor_.monitorWidth);
    const int bottom = std::clamp(static_cast<int>(std::ceil(wallpaperWindowPixels_.Bottom())) +
            wallpaperSamplingPaddingPixels_,
        top + 1, wallpaperMonitor_.monitorHeight);
    const WICRect crop{left, top, right - left, bottom - top};
    wallpaperBitmapPixels_ = {static_cast<float>(left), static_cast<float>(top),
        static_cast<float>(right - left), static_cast<float>(bottom - top)};
    Microsoft::WRL::ComPtr<IWICBitmapClipper> clipper;
    result = resources_->WicFactory()->CreateBitmapClipper(clipper.GetAddressOf());
    if (FAILED(result)) return result;
    result = clipper->Initialize(monitorBitmap.Get(), &crop);
    if (FAILED(result)) return result;
    return renderTarget_->CreateBitmapFromWicBitmap(clipper.Get(), nullptr, wallpaperBitmap_.GetAddressOf());
}

D2D1_RECT_F Renderer::ToD2D(RectF rect) const noexcept {
    return D2D1::RectF(rect.Left(), rect.Top(), rect.Right(), rect.Bottom());
}

void Renderer::DrawWallpaper(RectF destinationRect) {
    if (EnsureWallpaperBitmap() != S_OK || !wallpaperBitmap_) return;

    const D2D1_SIZE_F bitmapSize = wallpaperBitmap_->GetSize();
    if (bitmapSize.width <= 0.0f || bitmapSize.height <= 0.0f) {
        return;
    }

    const D2D1_RECT_F source = D2D1::RectF(0.0f, 0.0f, bitmapSize.width, bitmapSize.height);
    const float scaleX = destinationRect.width / std::max(1.0f, wallpaperWindowPixels_.width);
    const float scaleY = destinationRect.height / std::max(1.0f, wallpaperWindowPixels_.height);
    const RectF logicalDestination{
        destinationRect.x + (wallpaperBitmapPixels_.x - wallpaperWindowPixels_.x) * scaleX,
        destinationRect.y + (wallpaperBitmapPixels_.y - wallpaperWindowPixels_.y) * scaleY,
        wallpaperBitmapPixels_.width * scaleX,
        wallpaperBitmapPixels_.height * scaleY,
    };
    const D2D1_RECT_F destination = ToD2D(logicalDestination);
    D2D1_MATRIX_3X2_F transform{};
    renderTarget_->GetTransform(&transform);
    const auto transformed = [&transform](float x, float y) {
        return D2D1::Point2F(x * transform._11 + y * transform._21 + transform._31,
            x * transform._12 + y * transform._22 + transform._32);
    };
    const D2D1_POINT_2F topLeft = transformed(logicalDestination.Left(), logicalDestination.Top());
    const D2D1_POINT_2F bottomRight = transformed(logicalDestination.Right(), logicalDestination.Bottom());
    wallpaperDestination_ = {topLeft.x, topLeft.y,
        bottomRight.x - topLeft.x, bottomRight.y - topLeft.y};

    renderTarget_->DrawBitmap(
        wallpaperBitmap_.Get(),
        &destination,
        1.0f,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
        &source);
}

void Renderer::DrawGlass(const WidgetInstance& widget, RectF rect) {
    if (!wallpaperBitmap_ || widget.appearance.surface != SurfaceMode::Frosted ||
        widget.appearance.blurRadius <= 0.0f) return;
    D2D1_MATRIX_3X2_F transform{};
    renderTarget_->GetTransform(&transform);
    const auto transformPoint = [&transform](float x, float y) {
        return D2D1::Point2F(x * transform._11 + y * transform._21 + transform._31,
            x * transform._12 + y * transform._22 + transform._32);
    };
    const D2D1_POINT_2F topLeft = transformPoint(rect.Left(), rect.Top());
    const D2D1_POINT_2F bottomRight = transformPoint(rect.Right(), rect.Bottom());
    const RectF targetRect{topLeft.x, topLeft.y,
        std::max(1.0f, bottomRight.x - topLeft.x),
        std::max(1.0f, bottomRight.y - topLeft.y)};

    GlassCacheEntry& cache = glassCache_[widget.instanceId];
    if (!cache.bitmap || cache.wallpaperRevision != wallpaperRevision_ ||
        cache.blurRadius != widget.appearance.blurRadius || !SameRect(cache.targetRect, targetRect)) {
        const D2D1_SIZE_F bitmapSize = wallpaperBitmap_->GetSize();
        const float sourceScaleX = bitmapSize.width / std::max(1.0f, wallpaperDestination_.width);
        const float sourceScaleY = bitmapSize.height / std::max(1.0f, wallpaperDestination_.height);
        const D2D1_RECT_F source = D2D1::RectF(
            (targetRect.Left() - wallpaperDestination_.Left()) * sourceScaleX,
            (targetRect.Top() - wallpaperDestination_.Top()) * sourceScaleY,
            (targetRect.Right() - wallpaperDestination_.Left()) * sourceScaleX,
            (targetRect.Bottom() - wallpaperDestination_.Top()) * sourceScaleY);
        // Keep the intermediate at the final card size. The former implementation
        // made a tiny thumbnail and enlarged it, which changed apparent resolution.
        // Multiple symmetric, monitor-space samples soften detail without changing
        // the central wallpaper transform or scaling a blurred texture.
        const D2D1_SIZE_F blurSize = D2D1::SizeF(targetRect.width, targetRect.height);
        Microsoft::WRL::ComPtr<ID2D1BitmapRenderTarget> blurTarget;
        if (FAILED(renderTarget_->CreateCompatibleRenderTarget(&blurSize, nullptr, nullptr,
                D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE, blurTarget.GetAddressOf()))) return;
        blurTarget->BeginDraw();
        blurTarget->Clear(Color(0.0f, 0.0f, 0.0f, 0.0f));
        const D2D1_RECT_F destination = D2D1::RectF(0.0f, 0.0f, blurSize.width, blurSize.height);
        blurTarget->DrawBitmap(wallpaperBitmap_.Get(), &destination, 1.0f,
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &source);
        const float sampleOffsetX = std::max(0.75f,
            widget.appearance.blurRadius * sourceScaleX * 0.18f);
        const float sampleOffsetY = std::max(0.75f,
            widget.appearance.blurRadius * sourceScaleY * 0.18f);
        for (const D2D1_POINT_2F direction : {
                D2D1::Point2F(-1.0f, 0.0f), D2D1::Point2F(1.0f, 0.0f),
                D2D1::Point2F(0.0f, -1.0f), D2D1::Point2F(0.0f, 1.0f),
                D2D1::Point2F(-0.7f, -0.7f), D2D1::Point2F(0.7f, -0.7f),
                D2D1::Point2F(-0.7f, 0.7f), D2D1::Point2F(0.7f, 0.7f)}) {
            D2D1_RECT_F shifted = source;
            shifted.left += direction.x * sampleOffsetX;
            shifted.right += direction.x * sampleOffsetX;
            shifted.top += direction.y * sampleOffsetY;
            shifted.bottom += direction.y * sampleOffsetY;
            blurTarget->DrawBitmap(wallpaperBitmap_.Get(), &destination, 0.12f,
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &shifted);
        }
        if (FAILED(blurTarget->EndDraw())) return;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
        if (FAILED(blurTarget->GetBitmap(bitmap.GetAddressOf()))) return;
        cache = GlassCacheEntry{targetRect, widget.appearance.blurRadius, wallpaperRevision_, std::move(bitmap)};
    }

    Microsoft::WRL::ComPtr<ID2D1RoundedRectangleGeometry> mask;
    Microsoft::WRL::ComPtr<ID2D1Layer> layer;
    const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(
        ToD2D(rect), widget.appearance.cornerRadius, widget.appearance.cornerRadius);
    const bool masked = SUCCEEDED(resources_->D2DFactory()->CreateRoundedRectangleGeometry(
            &rounded, mask.GetAddressOf())) &&
        SUCCEEDED(renderTarget_->CreateLayer(nullptr, layer.GetAddressOf()));
    if (masked) renderTarget_->PushLayer(
        D2D1::LayerParameters(D2D1::InfiniteRect(), mask.Get()), layer.Get());
    const D2D1_RECT_F destination = ToD2D(rect);
    renderTarget_->DrawBitmap(cache.bitmap.Get(), &destination, 1.0f,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    if (masked) renderTarget_->PopLayer();
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
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> shadowBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> surfaceBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> borderBrush;

    const bool light = widget.appearance.mode == AppearanceMode::Light;
    if (FAILED(renderTarget_->CreateSolidColorBrush(
            Color(0.0f, 0.0f, 0.0f, light ? 0.14f : 0.22f), shadowBrush.GetAddressOf()))) {
        return;
    }
    if (FAILED(renderTarget_->CreateSolidColorBrush(
            light
                ? Color(0.94f, 0.95f, 0.96f, widget.appearance.opacity)
                : Color(0.085f, 0.09f, 0.105f, widget.appearance.opacity),
            surfaceBrush.GetAddressOf()))) {
        return;
    }
    if (FAILED(renderTarget_->CreateSolidColorBrush(
            light ? Color(0.0f, 0.0f, 0.0f, 0.12f) : Color(1.0f, 1.0f, 1.0f, 0.16f),
            borderBrush.GetAddressOf()))) {
        return;
    }

    const RectF shadowRect{rect.x, rect.y + WidgetVisualStyle::kShadowOffset, rect.width, rect.height};
    const D2D1_ROUNDED_RECT shadow = D2D1::RoundedRect(
        ToD2D(shadowRect), widget.appearance.cornerRadius, widget.appearance.cornerRadius);
    const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(
        ToD2D(rect),
        widget.appearance.cornerRadius,
        widget.appearance.cornerRadius);

    if (widget.appearance.shadowEnabled) renderTarget_->FillRoundedRectangle(&shadow, shadowBrush.Get());
    Microsoft::WRL::ComPtr<ID2D1RoundedRectangleGeometry> cardClip;
    Microsoft::WRL::ComPtr<ID2D1Layer> cardLayer;
    const bool roundedClip = SUCCEEDED(resources_->D2DFactory()->CreateRoundedRectangleGeometry(
            &rounded, cardClip.GetAddressOf())) &&
        SUCCEEDED(renderTarget_->CreateLayer(nullptr, cardLayer.GetAddressOf()));
    if (roundedClip) {
        renderTarget_->PushLayer(
            D2D1::LayerParameters(D2D1::InfiniteRect(), cardClip.Get()), cardLayer.Get());
    }
    DrawGlass(widget, rect);
    if (widget.appearance.surface != SurfaceMode::Transparent) {
        renderTarget_->FillRoundedRectangle(&rounded, surfaceBrush.Get());
    }
    if (widget.appearance.borderEnabled) {
        renderTarget_->DrawRoundedRectangle(&rounded, borderBrush.Get(), WidgetVisualStyle::kBorderWidth);
    }

    if (widget.content) {
        const RectF contentBounds = WidgetVisualStyle::ContentBounds(rect, widget.appearance.innerPadding);
        const D2D1_RECT_F contentClip = ToD2D(contentBounds);
        renderTarget_->PushAxisAlignedClip(&contentClip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        widget.content->Render(WidgetRenderContext{
            .renderTarget = *renderTarget_.Get(),
            .d2dFactory = *resources_->D2DFactory(),
            .dwriteFactory = *resources_->DWriteFactory(),
            .wicFactory = *resources_->WicFactory(),
            .titleFormat = *resources_->LabelFormat(),
            .detailFormat = *resources_->SmallFormat(),
            .bounds = contentBounds,
            .instanceId = widget.instanceId,
            .contentScale = widget.contentScale,
            .lightAppearance = widget.appearance.mode == AppearanceMode::Light,
            .resourceGeneration = resourceGeneration_,
        });
        renderTarget_->PopAxisAlignedClip();
    }
    if (roundedClip) renderTarget_->PopLayer();

    if (editMode && widget.selected) {
        DrawSelection(widget, rect);
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
    Microsoft::WRL::ComPtr<ID2D1StrokeStyle> secondaryStroke;
    if (!widget.primarySelection) {
        const D2D1_STROKE_STYLE_PROPERTIES properties{
            D2D1_CAP_STYLE_ROUND,
            D2D1_CAP_STYLE_ROUND,
            D2D1_CAP_STYLE_ROUND,
            D2D1_LINE_JOIN_ROUND,
            10.0f,
            D2D1_DASH_STYLE_DASH,
            0.0f,
        };
        static_cast<void>(resources_->D2DFactory()->CreateStrokeStyle(
            properties, nullptr, 0, secondaryStroke.GetAddressOf()));
    }
    renderTarget_->DrawRoundedRectangle(
        &rounded,
        brush.Get(),
        widget.primarySelection ? 2.0f : 1.5f,
        secondaryStroke.Get());
}

HRESULT Renderer::Render(
    const WidgetScene& scene,
    const GridLayout& layout,
    const GridMetrics& metrics,
    bool editMode,
    bool showGrid,
    float sceneScale,
    PointF sceneOffset,
    SizeF sceneSize,
    RectF wallpaperWindowPixels,
    WallpaperMonitorGeometry wallpaperMonitor,
    std::wstring_view monitorId) {

    HRESULT hr = CreateDeviceResources();
    if (FAILED(hr)) return hr;

    const D2D1_SIZE_F targetSize = renderTarget_->GetSize();
    if (sceneSize.width <= 0.0f || sceneSize.height <= 0.0f) {
        sceneSize = {targetSize.width, targetSize.height};
    }
    const RectF fullScene{0.0f, 0.0f, sceneSize.width, sceneSize.height};
    if (wallpaperWindowPixels.width <= 0.0f || wallpaperWindowPixels.height <= 0.0f) {
        wallpaperWindowPixels = {0.0f, 0.0f,
            static_cast<float>(wallpaperMonitor.monitorWidth),
            static_cast<float>(wallpaperMonitor.monitorHeight)};
    }
    if (!SameRect(wallpaperWindowPixels_, wallpaperWindowPixels) ||
        wallpaperMonitorId_ != monitorId ||
        wallpaperMonitor_.monitorWidth != wallpaperMonitor.monitorWidth ||
        wallpaperMonitor_.monitorHeight != wallpaperMonitor.monitorHeight) {
        wallpaperBitmap_.Reset();
        glassCache_.clear();
    }
    wallpaperWindowPixels_ = wallpaperWindowPixels;
    wallpaperMonitor_ = wallpaperMonitor;
    wallpaperMonitorId_ = monitorId;
    wallpaperSamplingPaddingPixels_ = 0;

    std::erase_if(glassCache_, [&scene](const auto& entry) {
        return std::none_of(scene.Widgets().begin(), scene.Widgets().end(),
            [&entry](const WidgetInstance& widget) { return widget.instanceId == entry.first; });
    });

    renderTarget_->BeginDraw();
    renderTarget_->Clear(Color(0.08f, 0.09f, 0.11f));
    renderTarget_->SetTransform(D2D1::Matrix3x2F(
        sceneScale, 0.0f, 0.0f, sceneScale, sceneOffset.x, sceneOffset.y));
    DrawWallpaper(fullScene);

    if (showGrid) {
        DrawGrid(layout, metrics);
    }

    for (const auto& widget : scene.Widgets()) {
        if (!monitorId.empty() && widget.monitorId != monitorId) continue;
        DrawWidget(widget, OuterLayout::RectFor(widget, layout, metrics), editMode);
    }
#ifdef _DEBUG
    if (editMode) {
        const auto selected = std::find_if(scene.Widgets().begin(), scene.Widgets().end(),
            [&monitorId](const WidgetInstance& widget) {
                return widget.primarySelection && (monitorId.empty() || widget.monitorId == monitorId);
            });
        if (selected != scene.Widgets().end()) {
            const RectF widgetRect = OuterLayout::RectFor(*selected, layout, metrics);
            const float pixelsPerDip = static_cast<float>(wallpaperMonitor.monitorWidth) /
                std::max(1.0f, sceneSize.width);
            const RectF sourcePixels{widgetRect.x * pixelsPerDip, widgetRect.y * pixelsPerDip,
                widgetRect.width * pixelsPerDip, widgetRect.height * pixelsPerDip};
            std::array<wchar_t, 320> diagnostic{};
            swprintf_s(diagnostic.data(), diagnostic.size(),
                L"Monitor: %d x %d\nDPI: %.0f / %.0f%%\nGrid: %d x %d\n"
                L"Widget: %.1f, %.1f, %.1f, %.1f DIP\nWallpaper source: %.1f, %.1f, %.1f, %.1f px",
                wallpaperMonitor.monitorWidth, wallpaperMonitor.monitorHeight,
                pixelsPerDip * 96.0f, pixelsPerDip * 100.0f,
                layout.Columns(), layout.Rows(), widgetRect.x, widgetRect.y,
                widgetRect.width, widgetRect.height, sourcePixels.x, sourcePixels.y,
                sourcePixels.width, sourcePixels.height);
            Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> background;
            Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> foreground;
            if (SUCCEEDED(renderTarget_->CreateSolidColorBrush(
                    Color(0.0f, 0.0f, 0.0f, 0.76f), background.GetAddressOf())) &&
                SUCCEEDED(renderTarget_->CreateSolidColorBrush(
                    Color(1.0f, 1.0f, 1.0f), foreground.GetAddressOf()))) {
                const D2D1_RECT_F panel = D2D1::RectF(24.0f, 24.0f, 390.0f, 130.0f);
                renderTarget_->FillRoundedRectangle(
                    D2D1::RoundedRect(panel, 8.0f, 8.0f), background.Get());
                const D2D1_RECT_F textBounds = D2D1::RectF(36.0f, 32.0f, 380.0f, 126.0f);
                renderTarget_->DrawTextW(diagnostic.data(),
                    static_cast<UINT32>(wcslen(diagnostic.data())), resources_->SmallFormat(),
                    &textBounds, foreground.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            }
        }
    }
#endif
    renderTarget_->SetTransform(D2D1::Matrix3x2F::Identity());

    hr = renderTarget_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        const HRESULT recreateResult = CreateDeviceResources();
        if (FAILED(recreateResult)) {
            return recreateResult;
        }
        return D2DERR_RECREATE_TARGET;
    }
    return hr;
}

HRESULT Renderer::RenderWidget(const WidgetInstance& widget, bool editMode,
    RectF windowBoundsOnMonitorPixels, WallpaperMonitorGeometry wallpaperMonitor,
    std::wstring_view monitorId, RectF widgetBoundsInWindow) {
    const int requestedPadding = widget.appearance.surface == SurfaceMode::Frosted
        ? static_cast<int>(std::ceil(widget.appearance.blurRadius * dpi_ / 96.0f)) + 2 : 0;
    if (!SameRect(wallpaperWindowPixels_, windowBoundsOnMonitorPixels) ||
        wallpaperMonitorId_ != monitorId ||
        wallpaperMonitor_.monitorWidth != wallpaperMonitor.monitorWidth ||
        wallpaperMonitor_.monitorHeight != wallpaperMonitor.monitorHeight ||
        wallpaperSamplingPaddingPixels_ != requestedPadding) {
        wallpaperBitmap_.Reset();
        glassCache_.clear();
    }
    wallpaperWindowPixels_ = windowBoundsOnMonitorPixels;
    wallpaperMonitor_ = wallpaperMonitor;
    wallpaperMonitorId_ = monitorId;
    wallpaperSamplingPaddingPixels_ = requestedPadding;
    HRESULT hr = CreateDeviceResources();
    if (FAILED(hr)) return hr;
    const D2D1_SIZE_F targetSize = renderTarget_->GetSize();
    const RectF targetBounds{0.0f, 0.0f, targetSize.width, targetSize.height};

    std::erase_if(glassCache_, [&widget](const auto& entry) {
        return entry.first != widget.instanceId;
    });

    renderTarget_->BeginDraw();
    renderTarget_->Clear(Color(0.92f, 0.91f, 0.89f));
    DrawWallpaper(targetBounds);
    renderTarget_->SetTransform(D2D1::Matrix3x2F::Identity());
    DrawWidget(widget, widgetBoundsInWindow, editMode);
    hr = renderTarget_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        const HRESULT recreateResult = CreateDeviceResources();
        if (FAILED(recreateResult)) return recreateResult;
        return D2DERR_RECREATE_TARGET;
    }
    return hr;
}

} // namespace ws
