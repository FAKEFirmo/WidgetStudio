#include "layout/OuterLayout.h"

#include <algorithm>
#include <cmath>

namespace ws {

FreePlacement OuterLayout::MoveFreeToPoint(
    FreePlacement placement, PointF pointer, PointF dragOffset, RectF bounds) noexcept {
    const float maximumX = std::max(bounds.x, bounds.Right() - placement.width);
    const float maximumY = std::max(bounds.y, bounds.Bottom() - placement.height);
    placement.x = std::clamp(pointer.x - dragOffset.x, bounds.x, maximumX);
    placement.y = std::clamp(pointer.y - dragOffset.y, bounds.y, maximumY);
    return placement;
}

GridPlacement OuterLayout::GridForRect(
    RectF rect, GridPlacement placement, const GridLayout& grid, const GridMetrics& metrics,
    int minimumColumns, int minimumRows, int maximumColumns, int maximumRows) noexcept {
    minimumColumns = std::clamp(minimumColumns, 1, grid.Columns());
    minimumRows = std::clamp(minimumRows, 1, grid.Rows());
    maximumColumns = std::clamp(maximumColumns, minimumColumns, grid.Columns());
    maximumRows = std::clamp(maximumRows, minimumRows, grid.Rows());
    const float stride = std::max(1.0f, metrics.cellSize + metrics.gap);
    placement.columnSpan = std::clamp(
        static_cast<int>(std::lround((rect.width + metrics.gap) / stride)),
        minimumColumns, maximumColumns);
    placement.rowSpan = std::clamp(
        static_cast<int>(std::lround((rect.height + metrics.gap) / stride)),
        minimumRows, maximumRows);
    return grid.MoveToPoint(placement, PointF{rect.x, rect.y}, PointF{}, metrics);
}

} // namespace ws
