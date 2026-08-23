#pragma once

#include "common/Geometry.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace ws {

struct AuthoredLayoutProfile {
    std::string_view id;
    float minimumAspect{};
    float maximumAspect{};
    SizeF referenceSize{};
};

struct AuthoredLayoutResult {
    std::size_t profileIndex{};
    float scale{1.0f};
    PointF origin{};
    SizeF renderedSize{};
};

class AuthoredContentLayout {
public:
    [[nodiscard]] static std::size_t SelectProfile(
        std::span<const AuthoredLayoutProfile> profiles,
        float aspectRatio,
        std::optional<std::size_t> currentProfile = std::nullopt,
        float hysteresis = 0.03f) noexcept;

    [[nodiscard]] static AuthoredLayoutResult Fit(
        std::span<const AuthoredLayoutProfile> profiles,
        RectF available,
        std::optional<std::size_t> currentProfile = std::nullopt,
        float contentScale = 1.0f,
        float hysteresis = 0.03f) noexcept;

    [[nodiscard]] static AuthoredLayoutResult FitReference(
        SizeF referenceSize, RectF available, float contentScale = 1.0f) noexcept;
};

} // namespace ws
