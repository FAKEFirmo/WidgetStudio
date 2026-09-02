#include "rendering/WallpaperPlacement.h"

#include <algorithm>

namespace ws {

WallpaperTransform WallpaperPlacement::Calculate(
    SizeF source, const WallpaperMonitorGeometry& monitor,
    WallpaperPosition position) noexcept {
    source.width = std::max(1.0f, source.width);
    source.height = std::max(1.0f, source.height);
    const float monitorWidth = static_cast<float>(std::max(1, monitor.monitorWidth));
    const float monitorHeight = static_cast<float>(std::max(1, monitor.monitorHeight));
    const float virtualWidth = static_cast<float>(std::max(1, monitor.virtualWidth));
    const float virtualHeight = static_cast<float>(std::max(1, monitor.virtualHeight));

    WallpaperTransform result{};
    if (position == WallpaperPosition::Tile) {
        result.tiled = true;
        return result;
    }

    float targetWidth = monitorWidth;
    float targetHeight = monitorHeight;
    if (position == WallpaperPosition::Span) {
        targetWidth = virtualWidth;
        targetHeight = virtualHeight;
        result.spansVirtualDesktop = true;
    }

    if (position == WallpaperPosition::Stretch) {
        result.scaleX = targetWidth / source.width;
        result.scaleY = targetHeight / source.height;
    } else if (position == WallpaperPosition::Center) {
        result.scaleX = 1.0f;
        result.scaleY = 1.0f;
    } else {
        const float uniform = position == WallpaperPosition::Fit
            ? std::min(targetWidth / source.width, targetHeight / source.height)
            : std::max(targetWidth / source.width, targetHeight / source.height);
        result.scaleX = uniform;
        result.scaleY = uniform;
    }

    result.destinationX = (targetWidth - source.width * result.scaleX) * 0.5f;
    result.destinationY = (targetHeight - source.height * result.scaleY) * 0.5f;
    if (position == WallpaperPosition::Fill && result.destinationY < 0.0f) {
        // Explorer's CropToFit composition keeps two thirds of the vertical
        // overflow below the viewport. Reproducing that shell rule is
        // required for glass widgets to sample the pixels behind them.
        result.destinationY *= 2.0f / 3.0f;
    }
    if (result.spansVirtualDesktop) {
        result.destinationX -= static_cast<float>(monitor.monitorX - monitor.virtualX);
        result.destinationY -= static_cast<float>(monitor.monitorY - monitor.virtualY);
    }
    return result;
}

RectF WallpaperPlacement::SourceRect(
    RectF destination, const WallpaperTransform& transform) noexcept {
    if (transform.tiled) return destination;
    return {
        (destination.x - transform.destinationX) / std::max(0.0001f, transform.scaleX),
        (destination.y - transform.destinationY) / std::max(0.0001f, transform.scaleY),
        destination.width / std::max(0.0001f, transform.scaleX),
        destination.height / std::max(0.0001f, transform.scaleY),
    };
}

} // namespace ws
