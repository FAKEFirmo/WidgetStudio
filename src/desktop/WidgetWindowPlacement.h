#pragma once

#include "common/Geometry.h"
#include "layout/GridLayout.h"

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

class WidgetWindowPlacementCalculator {
public:
    [[nodiscard]] static WidgetWindowPlacement Calculate(
        const WidgetInstance& widget,
        const GridLayout& grid,
        const GridMetrics& metrics,
        const MonitorDescriptor& monitor) noexcept;
};

} // namespace ws
