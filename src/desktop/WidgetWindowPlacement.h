#pragma once

#include "common/Geometry.h"
#include "layout/GridLayout.h"
#include "rendering/WallpaperPlacement.h"

namespace ws {

struct MonitorDescriptor;
struct WidgetInstance;

struct WidgetWindowPlacement {
    static constexpr float kRenderingMargin = 5.0f;

    RectF widgetDips{};
    RectF windowDips{};
    RectF widgetInWindowDips{};
    int screenX{};
    int screenY{};
    int pixelWidth{};
    int pixelHeight{};
};

struct WallpaperSamplingGeometry {
    RectF workAreaOnMonitorDips{};
    SizeF fullMonitorDips{};
};

class WidgetWindowPlacementCalculator {
public:
    [[nodiscard]] static WidgetWindowPlacement Calculate(
        const WidgetInstance& widget,
        const GridLayout& grid,
        const GridMetrics& metrics,
        const MonitorDescriptor& monitor) noexcept;
    [[nodiscard]] static WallpaperSamplingGeometry WallpaperSampling(
        const MonitorDescriptor& monitor) noexcept;
    [[nodiscard]] static WallpaperMonitorGeometry WallpaperMonitor(
        const MonitorDescriptor& monitor) noexcept;
    [[nodiscard]] static int DipsToPhysicalPixels(float dips, unsigned int dpi) noexcept;
    [[nodiscard]] static float PhysicalPixelsToDips(int pixels, unsigned int dpi) noexcept;
};

} // namespace ws
