#include "layout/GridLayout.h"

#include <algorithm>
#include <cmath>

namespace ws {

void GridLayout::SetDimensions(int columns, int rows) noexcept {
    columns_ = std::max(1, columns);
    rows_ = std::max(1, rows);
}

void GridLayout::SetGap(float gap) noexcept {
    gap_ = std::max(0.0f, gap);
}

void GridLayout::SetOuterMargin(float margin) noexcept {
    outerMargin_ = std::max(0.0f, margin);
}

GridMetrics GridLayout::Calculate(SizeF clientSize) const noexcept {
    GridMetrics metrics{};
    metrics.columns = columns_;
    metrics.rows = rows_;
    metrics.gap = gap_;

    const float availableWidth = std::max(
        1.0f,
        clientSize.width - 2.0f * outerMargin_ - gap_ * static_cast<float>(columns_ - 1));
    const float availableHeight = std::max(
        1.0f,
        clientSize.height - 2.0f * outerMargin_ - gap_ * static_cast<float>(rows_ - 1));

    const float cellFromWidth = availableWidth / static_cast<float>(columns_);
    const float cellFromHeight = availableHeight / static_cast<float>(rows_);
    metrics.cellSize = std::max(1.0f, std::min(cellFromWidth, cellFromHeight));

    metrics.contentWidth = metrics.cellSize * static_cast<float>(columns_) +
        gap_ * static_cast<float>(columns_ - 1);
    metrics.contentHeight = metrics.cellSize * static_cast<float>(rows_) +
        gap_ * static_cast<float>(rows_ - 1);

    // Center the square-cell grid inside the client area. This prevents the
    // grid from drifting when the development host is resized.
    metrics.originX = (clientSize.width - metrics.contentWidth) * 0.5f;
    metrics.originY = (clientSize.height - metrics.contentHeight) * 0.5f;

    return metrics;
}

RectF GridLayout::RectFor(const GridPlacement& placement, const GridMetrics& metrics) const noexcept {
    const int column = std::clamp(placement.column, 0, std::max(0, columns_ - 1));
    const int row = std::clamp(placement.row, 0, std::max(0, rows_ - 1));
    const int columnSpan = std::clamp(placement.columnSpan, 1, std::max(1, columns_ - column));
    const int rowSpan = std::clamp(placement.rowSpan, 1, std::max(1, rows_ - row));

    return RectF{
        metrics.originX + static_cast<float>(column) * (metrics.cellSize + metrics.gap),
        metrics.originY + static_cast<float>(row) * (metrics.cellSize + metrics.gap),
        static_cast<float>(columnSpan) * metrics.cellSize + static_cast<float>(columnSpan - 1) * metrics.gap,
        static_cast<float>(rowSpan) * metrics.cellSize + static_cast<float>(rowSpan - 1) * metrics.gap,
    };
}

GridPlacement GridLayout::MoveToPoint(
    const GridPlacement& placement,
    PointF pointer,
    PointF dragOffset,
    const GridMetrics& metrics) const noexcept {

    const float stride = metrics.cellSize + metrics.gap;
    const float desiredLeft = pointer.x - dragOffset.x;
    const float desiredTop = pointer.y - dragOffset.y;

    int column = static_cast<int>(std::lround((desiredLeft - metrics.originX) / stride));
    int row = static_cast<int>(std::lround((desiredTop - metrics.originY) / stride));

    column = std::clamp(column, 0, std::max(0, columns_ - placement.columnSpan));
    row = std::clamp(row, 0, std::max(0, rows_ - placement.rowSpan));

    GridPlacement moved = placement;
    moved.column = column;
    moved.row = row;
    return moved;
}

} // namespace ws
