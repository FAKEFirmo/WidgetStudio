#pragma once

#include "common/Geometry.h"

#include <d2d1.h>
#include <dwrite.h>

#include <string_view>

namespace ws {

struct WidgetRenderContext {
    ID2D1RenderTarget& renderTarget;
    IDWriteTextFormat& titleFormat;
    IDWriteTextFormat& detailFormat;
    RectF bounds;
    std::string_view instanceId;
    float contentScale{1.0f};
};

} // namespace ws
