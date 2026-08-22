#pragma once

#include <algorithm>

namespace ws {

struct PointF {
    float x{};
    float y{};
};

struct SizeF {
    float width{};
    float height{};
};

struct RectF {
    float x{};
    float y{};
    float width{};
    float height{};

    [[nodiscard]] float Left() const noexcept { return x; }
    [[nodiscard]] float Top() const noexcept { return y; }
    [[nodiscard]] float Right() const noexcept { return x + width; }
    [[nodiscard]] float Bottom() const noexcept { return y + height; }

    [[nodiscard]] bool Contains(PointF point) const noexcept {
        return point.x >= Left() && point.x <= Right() &&
               point.y >= Top() && point.y <= Bottom();
    }
};

inline float Clamp(float value, float low, float high) noexcept {
    return std::max(low, std::min(value, high));
}

} // namespace ws
