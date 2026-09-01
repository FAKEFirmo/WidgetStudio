#include "desktop/WidgetWindowPlacement.h"

#include "layout/OuterLayout.h"
#include "scene/WidgetInstance.h"
#include "windows/MonitorTopology.h"

#include <algorithm>
#include <cmath>

namespace ws {
namespace {

int DipToPixel(float value, unsigned int dpi) noexcept {
    return static_cast<int>(std::lround(value * static_cast<float>(std::max(1u, dpi)) / 96.0f));
}

} // namespace

WidgetWindowPlacement WidgetWindowPlacementCalculator::Calculate(
    const WidgetInstance& widget, const GridLayout& grid,
    const GridMetrics& metrics, const MonitorDescriptor& monitor) noexcept {
    const RectF rect = OuterLayout::RectFor(widget, grid, metrics);
    constexpr float margin = WidgetWindowPlacement::kRenderingMargin;
    const RectF windowRect{rect.x - margin, rect.y - margin,
        rect.width + margin * 2.0f, rect.height + margin * 2.0f};
    return WidgetWindowPlacement{
        .widgetDips = rect,
        .windowDips = windowRect,
        .widgetInWindowDips = {margin, margin, rect.width, rect.height},
        .screenX = monitor.monitorPixelX + DipToPixel(windowRect.x, monitor.dpi),
        .screenY = monitor.monitorPixelY + DipToPixel(windowRect.y, monitor.dpi),
        .pixelWidth = std::max(1, DipToPixel(windowRect.width, monitor.dpi)),
        .pixelHeight = std::max(1, DipToPixel(windowRect.height, monitor.dpi)),
    };
}

WallpaperSamplingGeometry WidgetWindowPlacementCalculator::WallpaperSampling(
    const MonitorDescriptor& monitor) noexcept {
    const float pixelsToDips = 96.0f / static_cast<float>(std::max(1u, monitor.dpi));
    return WallpaperSamplingGeometry{
        .workAreaOnMonitorDips = monitor.workAreaOnMonitorDips,
        .fullMonitorDips = {
            static_cast<float>(std::max(1, monitor.monitorPixelWidth)) * pixelsToDips,
            static_cast<float>(std::max(1, monitor.monitorPixelHeight)) * pixelsToDips,
        },
    };
}

WallpaperMonitorGeometry WidgetWindowPlacementCalculator::WallpaperMonitor(
    const MonitorDescriptor& monitor) noexcept {
    return {
        .monitorX = monitor.monitorPixelX,
        .monitorY = monitor.monitorPixelY,
        .monitorWidth = monitor.monitorPixelWidth,
        .monitorHeight = monitor.monitorPixelHeight,
        .virtualX = monitor.virtualPixelX,
        .virtualY = monitor.virtualPixelY,
        .virtualWidth = monitor.virtualPixelWidth,
        .virtualHeight = monitor.virtualPixelHeight,
    };
}

int WidgetWindowPlacementCalculator::DipsToPhysicalPixels(
    float dips, unsigned int dpi) noexcept {
    return DipToPixel(dips, dpi);
}

float WidgetWindowPlacementCalculator::PhysicalPixelsToDips(
    int pixels, unsigned int dpi) noexcept {
    return static_cast<float>(pixels) * 96.0f /
        static_cast<float>(std::max(1u, dpi));
}

} // namespace ws
