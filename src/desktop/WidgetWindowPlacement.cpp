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
        .screenX = monitor.pixelX + DipToPixel(windowRect.x, monitor.dpi),
        .screenY = monitor.pixelY + DipToPixel(windowRect.y, monitor.dpi),
        .pixelWidth = std::max(1, DipToPixel(windowRect.width, monitor.dpi)),
        .pixelHeight = std::max(1, DipToPixel(windowRect.height, monitor.dpi)),
    };
}

} // namespace ws
