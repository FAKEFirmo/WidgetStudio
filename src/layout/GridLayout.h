#pragma once

#include "common/Geometry.h"
#include "scene/WidgetInstance.h"

namespace ws {

struct GridMetrics {
    int columns{12};
    int rows{7};
    float cellSize{};
    float columnGap{10.0f};
    float rowGap{10.0f};
    float originX{};
    float originY{};
    float contentWidth{};
    float contentHeight{};
};

class GridLayout {
public:
    GridLayout() = default;

    void SetDimensions(int columns, int rows) noexcept;
    void SetGap(float gap) noexcept;
    void SetOuterMargin(float margin) noexcept;

    [[nodiscard]] int Columns() const noexcept { return columns_; }
    [[nodiscard]] int Rows() const noexcept { return rows_; }
    [[nodiscard]] float Gap() const noexcept { return gap_; }
    [[nodiscard]] float OuterMargin() const noexcept { return outerMargin_; }

    [[nodiscard]] GridMetrics Calculate(SizeF clientSize) const noexcept;
    [[nodiscard]] RectF RectFor(const GridPlacement& placement, const GridMetrics& metrics) const noexcept;

    [[nodiscard]] GridPlacement MoveToPoint(
        const GridPlacement& placement,
        PointF pointer,
        PointF dragOffset,
        const GridMetrics& metrics) const noexcept;

private:
    int columns_{12};
    int rows_{7};
    float gap_{10.0f};
    float outerMargin_{18.0f};
};

} // namespace ws
