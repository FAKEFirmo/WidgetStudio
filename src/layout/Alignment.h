#pragma once

#include "common/Geometry.h"
#include "scene/WidgetInstance.h"

#include <span>

namespace ws {

enum class AlignmentOperation {
    Left,
    HorizontalCenter,
    Right,
    Top,
    VerticalCenter,
    Bottom,
    MatchWidth,
    MatchHeight,
    MatchBoth,
    DistributeHorizontally,
    DistributeVertically,
};

struct AlignmentItem {
    FreePlacement* placement{};
    bool primary{false};
    bool locked{false};
};

class Alignment {
public:
    [[nodiscard]] static bool Apply(
        std::span<AlignmentItem> items, AlignmentOperation operation, RectF bounds) noexcept;
};

} // namespace ws
