#include "layout/AuthoredContentLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ws {

std::size_t AuthoredContentLayout::SelectProfile(
    std::span<const AuthoredLayoutProfile> profiles,
    float aspectRatio,
    std::optional<std::size_t> currentProfile,
    float hysteresis) noexcept {
    if (profiles.empty()) return 0;
    const float aspect = std::isfinite(aspectRatio) && aspectRatio > 0.0f ? aspectRatio : 1.0f;
    const float band = std::max(0.0f, hysteresis);
    if (currentProfile && *currentProfile < profiles.size()) {
        const auto& current = profiles[*currentProfile];
        const bool aboveMinimum = !std::isfinite(current.minimumAspect) || aspect >= current.minimumAspect - band;
        const bool belowMaximum = !std::isfinite(current.maximumAspect) || aspect < current.maximumAspect + band;
        if (aboveMinimum && belowMaximum) return *currentProfile;
    }
    for (std::size_t index = 0; index < profiles.size(); ++index) {
        const auto& profile = profiles[index];
        if (aspect >= profile.minimumAspect && aspect < profile.maximumAspect) return index;
    }
    return aspect < profiles.front().minimumAspect ? 0 : profiles.size() - 1;
}

AuthoredLayoutResult AuthoredContentLayout::Fit(
    std::span<const AuthoredLayoutProfile> profiles,
    RectF available,
    std::optional<std::size_t> currentProfile,
    float contentScale,
    float hysteresis) noexcept {
    if (profiles.empty()) return FitReference(SizeF{1.0f, 1.0f}, available, contentScale);
    const float aspect = available.width / std::max(1.0f, available.height);
    const std::size_t index = SelectProfile(profiles, aspect, currentProfile, hysteresis);
    AuthoredLayoutResult result = FitReference(profiles[index].referenceSize, available, contentScale);
    result.profileIndex = index;
    return result;
}

AuthoredLayoutResult AuthoredContentLayout::FitReference(
    SizeF referenceSize, RectF available, float contentScale) noexcept {
    const float referenceWidth = std::max(1.0f, referenceSize.width);
    const float referenceHeight = std::max(1.0f, referenceSize.height);
    const float availableWidth = std::max(0.0f, available.width);
    const float availableHeight = std::max(0.0f, available.height);
    const float fitScale = std::min(availableWidth / referenceWidth, availableHeight / referenceHeight);
    const float scale = fitScale * std::clamp(contentScale, 0.25f, 4.0f);
    const SizeF rendered{referenceWidth * scale, referenceHeight * scale};
    return AuthoredLayoutResult{
        .profileIndex = 0,
        .scale = scale,
        .origin = PointF{
            available.x + (available.width - rendered.width) * 0.5f,
            available.y + (available.height - rendered.height) * 0.5f,
        },
        .renderedSize = rendered,
    };
}

} // namespace ws
