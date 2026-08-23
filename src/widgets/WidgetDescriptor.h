#pragma once

#include "widgets/IWidget.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace ws {

struct GridSize {
    int columns{1};
    int rows{1};
};

enum class WidgetCapability : std::uint32_t {
    None = 0,
    Configurable = 1u << 0,
    Scalable = 1u << 1,
    Interactive = 1u << 2,
};

constexpr WidgetCapability operator|(WidgetCapability left, WidgetCapability right) noexcept {
    return static_cast<WidgetCapability>(
        static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

constexpr bool HasCapability(WidgetCapability value, WidgetCapability flag) noexcept {
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0;
}

struct WidgetDescriptor {
    std::string typeId;
    std::wstring displayName;
    std::wstring description;
    GridSize defaultGridSize{};
    GridSize minimumGridSize{};
    GridSize maximumGridSize{};
    WidgetCapability capabilities{WidgetCapability::None};
    std::function<std::unique_ptr<IWidget>()> factory;
};

} // namespace ws
