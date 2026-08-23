#pragma once

#include "common/Geometry.h"

namespace ws {

enum class PhotoFitMode { Fill, Fit };

struct PhotoLayoutResult {
    RectF destination{};
    RectF source{};
};

class PhotoLayout {
public:
    [[nodiscard]] static PhotoLayoutResult Calculate(
        SizeF imageSize, RectF target, PhotoFitMode mode, float focalX, float focalY) noexcept;
};

} // namespace ws
