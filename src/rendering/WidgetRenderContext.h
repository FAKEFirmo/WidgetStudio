#pragma once

#include "common/Geometry.h"

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>

#include <string_view>
#include <cstdint>

namespace ws {

struct WidgetRenderContext {
    ID2D1RenderTarget& renderTarget;
    ID2D1Factory& d2dFactory;
    IDWriteFactory& dwriteFactory;
    IWICImagingFactory& wicFactory;
    IDWriteTextFormat& titleFormat;
    IDWriteTextFormat& detailFormat;
    RectF bounds;
    std::string_view instanceId;
    float contentScale{1.0f};
    bool lightAppearance{false};
    std::uint64_t resourceGeneration{};
};

} // namespace ws
