#include "layout/Alignment.h"

#include <algorithm>
#include <vector>

namespace ws {
namespace {

void ClampPlacement(FreePlacement& placement, RectF bounds) noexcept {
    placement.width = std::clamp(placement.width, 1.0f, std::max(1.0f, bounds.width));
    placement.height = std::clamp(placement.height, 1.0f, std::max(1.0f, bounds.height));
    placement.x = std::clamp(placement.x, bounds.x, std::max(bounds.x, bounds.Right() - placement.width));
    placement.y = std::clamp(placement.y, bounds.y, std::max(bounds.y, bounds.Bottom() - placement.height));
}

} // namespace

bool Alignment::Apply(std::span<AlignmentItem> items, AlignmentOperation operation, RectF bounds) noexcept {
    const auto primary = std::find_if(items.begin(), items.end(),
        [](const AlignmentItem& item) { return item.primary && item.placement; });
    if (primary == items.end()) return false;
    const FreePlacement reference = *primary->placement;

    if (operation == AlignmentOperation::DistributeHorizontally ||
        operation == AlignmentOperation::DistributeVertically) {
        std::vector<FreePlacement*> placements;
        for (const AlignmentItem& item : items) {
            if (item.placement && !item.locked) placements.push_back(item.placement);
        }
        if (placements.size() < 3) return false;
        const bool horizontal = operation == AlignmentOperation::DistributeHorizontally;
        std::sort(placements.begin(), placements.end(), [horizontal](const auto* left, const auto* right) {
            return horizontal ? left->x < right->x : left->y < right->y;
        });
        const float first = horizontal ? placements.front()->x : placements.front()->y;
        const float lastEdge = horizontal
            ? placements.back()->x + placements.back()->width
            : placements.back()->y + placements.back()->height;
        float totalSize{};
        for (const auto* placement : placements) totalSize += horizontal ? placement->width : placement->height;
        const float gap = (lastEdge - first - totalSize) / static_cast<float>(placements.size() - 1);
        float cursor = first;
        for (auto* placement : placements) {
            if (horizontal) { placement->x = cursor; cursor += placement->width + gap; }
            else { placement->y = cursor; cursor += placement->height + gap; }
            ClampPlacement(*placement, bounds);
        }
        return true;
    }

    bool changed = false;
    for (AlignmentItem& item : items) {
        if (!item.placement || item.primary || item.locked) continue;
        FreePlacement& placement = *item.placement;
        switch (operation) {
        case AlignmentOperation::Left: placement.x = reference.x; break;
        case AlignmentOperation::HorizontalCenter:
            placement.x = reference.x + (reference.width - placement.width) * 0.5f; break;
        case AlignmentOperation::Right:
            placement.x = reference.x + reference.width - placement.width; break;
        case AlignmentOperation::Top: placement.y = reference.y; break;
        case AlignmentOperation::VerticalCenter:
            placement.y = reference.y + (reference.height - placement.height) * 0.5f; break;
        case AlignmentOperation::Bottom:
            placement.y = reference.y + reference.height - placement.height; break;
        case AlignmentOperation::MatchWidth: placement.width = reference.width; break;
        case AlignmentOperation::MatchHeight: placement.height = reference.height; break;
        case AlignmentOperation::MatchBoth:
            placement.width = reference.width; placement.height = reference.height; break;
        case AlignmentOperation::DistributeHorizontally:
        case AlignmentOperation::DistributeVertically: break;
        }
        ClampPlacement(placement, bounds);
        changed = true;
    }
    return changed;
}

} // namespace ws
