#pragma once

#include "common/Geometry.h"

namespace ws {

enum class WallpaperPosition {
    Center,
    Tile,
    Stretch,
    Fit,
    Fill,
    Span,
};

struct WallpaperMonitorGeometry {
    int monitorX{};
    int monitorY{};
    int monitorWidth{};
    int monitorHeight{};
    int virtualX{};
    int virtualY{};
    int virtualWidth{};
    int virtualHeight{};
};

struct WallpaperTransform {
    float scaleX{1.0f};
    float scaleY{1.0f};
    float destinationX{};
    float destinationY{};
    bool tiled{};
    bool spansVirtualDesktop{};
};

class WallpaperPlacement {
public:
    [[nodiscard]] static WallpaperTransform Calculate(
        SizeF sourcePixels,
        const WallpaperMonitorGeometry& monitor,
        WallpaperPosition position) noexcept;

    // destinationPixels is monitor-local. The returned rectangle is expressed
    // in original wallpaper pixels before clipping at the bitmap edges.
    [[nodiscard]] static RectF SourceRect(
        RectF destinationPixels,
        const WallpaperTransform& transform) noexcept;
};

} // namespace ws
