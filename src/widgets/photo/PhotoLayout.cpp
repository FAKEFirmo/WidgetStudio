#include "widgets/photo/PhotoLayout.h"

#include <algorithm>

namespace ws {

PhotoLayoutResult PhotoLayout::Calculate(
    SizeF imageSize, RectF target, PhotoFitMode mode, float focalX, float focalY) noexcept {
    const float imageWidth = std::max(1.0f, imageSize.width);
    const float imageHeight = std::max(1.0f, imageSize.height);
    const float targetWidth = std::max(1.0f, target.width);
    const float targetHeight = std::max(1.0f, target.height);
    PhotoLayoutResult result{
        .destination = target,
        .source = RectF{0.0f, 0.0f, imageWidth, imageHeight},
    };
    const float targetAspect = targetWidth / targetHeight;
    const float imageAspect = imageWidth / imageHeight;
    if (mode == PhotoFitMode::Fit) {
        const float scale = std::min(targetWidth / imageWidth, targetHeight / imageHeight);
        result.destination.width = imageWidth * scale;
        result.destination.height = imageHeight * scale;
        result.destination.x = target.x + (target.width - result.destination.width) * 0.5f;
        result.destination.y = target.y + (target.height - result.destination.height) * 0.5f;
    } else if (imageAspect > targetAspect) {
        result.source.width = imageHeight * targetAspect;
        result.source.x = (imageWidth - result.source.width) * std::clamp(focalX, 0.0f, 1.0f);
    } else {
        result.source.height = imageWidth / targetAspect;
        result.source.y = (imageHeight - result.source.height) * std::clamp(focalY, 0.0f, 1.0f);
    }
    return result;
}

} // namespace ws
