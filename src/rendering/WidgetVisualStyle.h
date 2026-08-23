#pragma once

#include "common/Geometry.h"

#include <algorithm>

namespace ws {

struct WidgetVisualStyle {
    static constexpr float kInnerPadding = 20.0f;
    static constexpr float kBorderWidth = 1.0f;
    static constexpr float kShadowOffset = 3.0f;

    [[nodiscard]] static RectF ContentBounds(RectF outer) noexcept {
        const float horizontal = std::min(kInnerPadding, outer.width * 0.25f);
        const float vertical = std::min(kInnerPadding, outer.height * 0.25f);
        return RectF{
            outer.x + horizontal,
            outer.y + vertical,
            std::max(0.0f, outer.width - horizontal * 2.0f),
            std::max(0.0f, outer.height - vertical * 2.0f),
        };
    }
};

} // namespace ws
