#pragma once

#include "layout/GridLayout.h"
#include "scene/WidgetInstance.h"

namespace ws {

class OuterLayout {
public:
    [[nodiscard]] static RectF RectFor(
        const WidgetInstance& widget, const GridLayout& grid, const GridMetrics& metrics) noexcept {
        if (widget.layoutMode == LayoutMode::Free) {
            return RectF{widget.free.x, widget.free.y, widget.free.width, widget.free.height};
        }
        return grid.RectFor(widget.grid, metrics);
    }

    [[nodiscard]] static FreePlacement MoveFreeToPoint(
        FreePlacement placement, PointF pointer, PointF dragOffset, RectF bounds) noexcept;
};

} // namespace ws
