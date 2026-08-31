#include "rendering/Renderer.h"
#include "layout/OuterLayout.h"
#include "rendering/RenderingResources.h"
#include "rendering/WidgetRenderContext.h"
#include "rendering/WidgetVisualStyle.h"
#include "rendering/WallpaperCache.h"

#include <algorithm>
#include <array>
#include <utility>

namespace ws {
namespace {

D2D1_COLOR_F Color(float r, float g, float b, float a = 1.0f) {
    return D2D1::ColorF(r, g, b, a);
}

D2D1_RECT_F WallpaperSourceRect(D2D1_SIZE_F targetSize, D2D1_SIZE_F bitmapSize) {
    const float targetAspect = targetSize.width / std::max(1.0f, targetSize.height);
    const float bitmapAspect = bitmapSize.width / std::max(1.0f, bitmapSize.height);
    if (bitmapAspect > targetAspect) {
        const float sourceWidth = bitmapSize.height * targetAspect;
        const float left = (bitmapSize.width - sourceWidth) * 0.5f;
        return D2D1::RectF(left, 0.0f, left + sourceWidth, bitmapSize.height);
    }
    const float sourceHeight = bitmapSize.width / targetAspect;
    const float top = (bitmapSize.height - sourceHeight) * 0.5f;
    return D2D1::RectF(0.0f, top, bitmapSize.width, top + sourceHeight);
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

    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const D2D1_SIZE_U size = D2D1::SizeU(
        static_cast<UINT32>(std::max<LONG>(1, rc.right - rc.left)),
        static_cast<UINT32>(std::max<LONG>(1, rc.bottom - rc.top)));

    const HRESULT hr = resources_->D2DFactory()->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
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
    return renderTarget_->Resize(D2D1::SizeU(std::max(1u, width), std::max(1u, height)));
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
    if (!wallpaperCache_ || !wallpaperCache_->Bitmap()) return S_FALSE;
    if (wallpaperRevision_ != wallpaperCache_->Revision()) {
        wallpaperBitmap_.Reset();
        glassCache_.clear();
        wallpaperRevision_ = wallpaperCache_->Revision();
    }
    if (wallpaperBitmap_) return S_OK;

    UINT sourceWidth{};
    UINT sourceHeight{};
    HRESULT result = wallpaperCache_->Bitmap()->GetSize(&sourceWidth, &sourceHeight);
    if (FAILED(result) || sourceWidth == 0 || sourceHeight == 0) return FAILED(result) ? result : E_FAIL;
    const D2D1_SIZE_F targetSize = renderTarget_->GetSize();
    const D2D1_SIZE_F desktopSize = wallpaperDesktopSize_.width > 0.0f && wallpaperDesktopSize_.height > 0.0f
        ? D2D1::SizeF(wallpaperDesktopSize_.width, wallpaperDesktopSize_.height) : targetSize;
    const D2D1_RECT_F fullSource = WallpaperSourceRect(
        desktopSize, D2D1::SizeF(static_cast<float>(sourceWidth), static_cast<float>(sourceHeight)));
    D2D1_RECT_F region = fullSource;
    if (wallpaperWindowBounds_.width > 0.0f && wallpaperWindowBounds_.height > 0.0f) {
        const float scaleX = (fullSource.right - fullSource.left) / std::max(1.0f, desktopSize.width);
        const float scaleY = (fullSource.bottom - fullSource.top) / std::max(1.0f, desktopSize.height);
        region = D2D1::RectF(
            fullSource.left + wallpaperWindowBounds_.Left() * scaleX,
            fullSource.top + wallpaperWindowBounds_.Top() * scaleY,
            fullSource.left + wallpaperWindowBounds_.Right() * scaleX,
            fullSource.top + wallpaperWindowBounds_.Bottom() * scaleY);
    }
    const int left = std::clamp(static_cast<int>(std::floor(region.left)), 0, static_cast<int>(sourceWidth) - 1);
    const int top = std::clamp(static_cast<int>(std::floor(region.top)), 0, static_cast<int>(sourceHeight) - 1);
    const int right = std::clamp(static_cast<int>(std::ceil(region.right)), left + 1, static_cast<int>(sourceWidth));
    const int bottom = std::clamp(static_cast<int>(std::ceil(region.bottom)), top + 1, static_cast<int>(sourceHeight));
    const WICRect crop{left, top, right - left, bottom - top};
    Microsoft::WRL::ComPtr<IWICBitmapClipper> clipper;
    result = resources_->WicFactory()->CreateBitmapClipper(clipper.GetAddressOf());
    if (FAILED(result)) return result;
    result = clipper->Initialize(wallpaperCache_->Bitmap(), &crop);
    if (FAILED(result)) return result;
    return renderTarget_->CreateBitmapFromWicBitmap(clipper.Get(), nullptr, wallpaperBitmap_.GetAddressOf());
}

D2D1_RECT_F Renderer::ToD2D(RectF rect) const noexcept {
    return D2D1::RectF(rect.Left(), rect.Top(), rect.Right(), rect.Bottom());
}

void Renderer::DrawWallpaper() {
    const D2D1_SIZE_F targetSize = renderTarget_->GetSize();
    const D2D1_RECT_F destination = D2D1::RectF(0.0f, 0.0f, targetSize.width, targetSize.height);

    if (EnsureWallpaperBitmap() != S_OK || !wallpaperBitmap_) {
        renderTarget_->Clear(Color(0.92f, 0.91f, 0.89f));
        return;
    }

    const D2D1_SIZE_F bitmapSize = wallpaperBitmap_->GetSize();
    if (bitmapSize.width <= 0.0f || bitmapSize.height <= 0.0f) {
        renderTarget_->Clear(Color(0.92f, 0.91f, 0.89f));
        return;
    }

    const D2D1_RECT_F source = D2D1::RectF(0.0f, 0.0f, bitmapSize.width, bitmapSize.height);

    renderTarget_->DrawBitmap(
        wallpaperBitmap_.Get(),
        &destination,
        1.0f,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
        &source);
}

void Renderer::DrawGlass(const WidgetInstance& widget, RectF rect) {
    if (!wallpaperBitmap_ || !widget.appearance.glassEnabled || widget.appearance.blurRadius <= 0.0f) return;
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
        const D2D1_SIZE_F targetSize = renderTarget_->GetSize();
        const D2D1_SIZE_F bitmapSize = wallpaperBitmap_->GetSize();
        const float sourceScaleX = bitmapSize.width / std::max(1.0f, targetSize.width);
        const float sourceScaleY = bitmapSize.height / std::max(1.0f, targetSize.height);
        const D2D1_RECT_F source = D2D1::RectF(
            targetRect.Left() * sourceScaleX,
            targetRect.Top() * sourceScaleY,
            targetRect.Right() * sourceScaleX,
            targetRect.Bottom() * sourceScaleY);
        const float downsample = std::clamp(1.0f + widget.appearance.blurRadius / 3.0f, 2.0f, 12.0f);
        const D2D1_SIZE_F smallSize = D2D1::SizeF(
            std::max(1.0f, targetRect.width / downsample),
            std::max(1.0f, targetRect.height / downsample));
        Microsoft::WRL::ComPtr<ID2D1BitmapRenderTarget> smallTarget;
        if (FAILED(renderTarget_->CreateCompatibleRenderTarget(&smallSize, nullptr, nullptr,
                D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE, smallTarget.GetAddressOf()))) return;
        smallTarget->BeginDraw();
        const D2D1_RECT_F destination = D2D1::RectF(0.0f, 0.0f, smallSize.width, smallSize.height);
        smallTarget->DrawBitmap(wallpaperBitmap_.Get(), &destination, 1.0f,
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &source);
        if (FAILED(smallTarget->EndDraw())) return;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
        if (FAILED(smallTarget->GetBitmap(bitmap.GetAddressOf()))) return;
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

    renderTarget_->FillRoundedRectangle(&shadow, shadowBrush.Get());
    DrawGlass(widget, rect);
    renderTarget_->FillRoundedRectangle(&rounded, surfaceBrush.Get());
    renderTarget_->DrawRoundedRectangle(&rounded, borderBrush.Get(), WidgetVisualStyle::kBorderWidth);

    if (widget.content) {
        const RectF contentBounds = WidgetVisualStyle::ContentBounds(rect);
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
    float sceneScale,
    PointF sceneOffset,
    std::wstring_view monitorId) {

    wallpaperWindowBounds_ = {};
    wallpaperDesktopSize_ = {};
    HRESULT hr = CreateDeviceResources();
    if (FAILED(hr)) return hr;

    std::erase_if(glassCache_, [&scene](const auto& entry) {
        return std::none_of(scene.Widgets().begin(), scene.Widgets().end(),
            [&entry](const WidgetInstance& widget) { return widget.instanceId == entry.first; });
    });

    renderTarget_->BeginDraw();
    DrawWallpaper();
    renderTarget_->SetTransform(D2D1::Matrix3x2F(
        sceneScale, 0.0f, 0.0f, sceneScale, sceneOffset.x, sceneOffset.y));

    if (editMode) {
        DrawGrid(layout, metrics);
    }

    for (const auto& widget : scene.Widgets()) {
        if (!monitorId.empty() && widget.monitorId != monitorId) continue;
        DrawWidget(widget, OuterLayout::RectFor(widget, layout, metrics), editMode);
    }
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
    RectF windowBoundsOnMonitor, SizeF monitorSize, RectF widgetBoundsInWindow) {
    if (!SameRect(wallpaperWindowBounds_, windowBoundsOnMonitor) ||
        wallpaperDesktopSize_.width != monitorSize.width || wallpaperDesktopSize_.height != monitorSize.height) {
        wallpaperBitmap_.Reset();
        glassCache_.clear();
    }
    wallpaperWindowBounds_ = windowBoundsOnMonitor;
    wallpaperDesktopSize_ = monitorSize;
    HRESULT hr = CreateDeviceResources();
    if (FAILED(hr)) return hr;

    std::erase_if(glassCache_, [&widget](const auto& entry) {
        return entry.first != widget.instanceId;
    });

    renderTarget_->BeginDraw();
    DrawWallpaper();
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
