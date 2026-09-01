#pragma once

#include "common/Geometry.h"
#include "layout/GridLayout.h"
#include "rendering/WallpaperPlacement.h"
#include "scene/WidgetScene.h"

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <windows.h>

#include <string>
#include <string_view>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace ws {

class WallpaperCache;
class RenderingResources;

class Renderer {
public:
    explicit Renderer(std::shared_ptr<WallpaperCache> wallpaperCache = {},
        std::shared_ptr<RenderingResources> resources = {});
    ~Renderer() = default;

    void SetWallpaperCache(std::shared_ptr<WallpaperCache> wallpaperCache);
    void SetSharedResources(std::shared_ptr<RenderingResources> resources);

    HRESULT Initialize(HWND hwnd);
    void DiscardDeviceResources() noexcept;
    HRESULT Resize(UINT width, UINT height);
    void SetDpi(float dpi) noexcept;

    HRESULT ReloadWallpaper();

    HRESULT Render(
        const WidgetScene& scene,
        const GridLayout& layout,
        const GridMetrics& metrics,
        bool editMode,
        bool showGrid = true,
        float sceneScale = 1.0f,
        PointF sceneOffset = {},
        SizeF sceneSize = {},
        RectF wallpaperWindowPixels = {},
        WallpaperMonitorGeometry wallpaperMonitor = {},
        std::wstring_view monitorId = {});

    HRESULT RenderWidget(
        const WidgetInstance& widget,
        bool editMode,
        RectF windowBoundsOnMonitorPixels,
        WallpaperMonitorGeometry wallpaperMonitor,
        std::wstring_view monitorId,
        RectF widgetBoundsInWindow);

private:
    HRESULT CreateDeviceResources();
    HRESULT EnsureWallpaperBitmap();

    void DrawWallpaper(RectF destination);
    void DrawGlass(const WidgetInstance& widget, RectF rect);
    void DrawGrid(const GridLayout& layout, const GridMetrics& metrics);
    void DrawWidget(const WidgetInstance& widget, RectF rect, bool editMode);
    void DrawSelection(const WidgetInstance& widget, RectF rect);

    D2D1_RECT_F ToD2D(RectF rect) const noexcept;

    HWND hwnd_{};
    float dpi_{96.0f};

    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> renderTarget_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> wallpaperBitmap_;
    std::shared_ptr<RenderingResources> resources_;
    std::shared_ptr<WallpaperCache> wallpaperCache_;
    std::uint64_t resourceGeneration_{};
    std::uint64_t wallpaperRevision_{};
    RectF wallpaperWindowPixels_{};
    RectF wallpaperBitmapPixels_{};
    WallpaperMonitorGeometry wallpaperMonitor_{};
    std::wstring wallpaperMonitorId_;
    RectF wallpaperDestination_{};
    int wallpaperSamplingPaddingPixels_{};

    struct GlassCacheEntry {
        RectF targetRect{};
        float blurRadius{};
        std::uint64_t wallpaperRevision{};
        Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    };
    std::unordered_map<std::string, GlassCacheEntry> glassCache_;
};

} // namespace ws
