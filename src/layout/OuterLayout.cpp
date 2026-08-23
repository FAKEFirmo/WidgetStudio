#include "layout/OuterLayout.h"

#include <algorithm>

namespace ws {

FreePlacement OuterLayout::MoveFreeToPoint(
    FreePlacement placement, PointF pointer, PointF dragOffset, RectF bounds) noexcept {
    const float maximumX = std::max(bounds.x, bounds.Right() - placement.width);
    const float maximumY = std::max(bounds.y, bounds.Bottom() - placement.height);
    placement.x = std::clamp(pointer.x - dragOffset.x, bounds.x, maximumX);
    placement.y = std::clamp(pointer.y - dragOffset.y, bounds.y, maximumY);
    return placement;
}

} // namespace ws
