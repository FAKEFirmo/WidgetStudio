#pragma once

#include "widgets/IWidget.h"

#include <memory>
#include <string>

namespace ws {

enum class LayoutMode {
    Grid,
    Free,
};

struct GridPlacement {
    int column{};
    int row{};
    int columnSpan{1};
    int rowSpan{1};
};

struct FreePlacement {
    float x{};
    float y{};
    float width{320.0f};
    float height{180.0f};
};

enum class AppearanceMode {
    Dark,
    Light,
};

struct WidgetAppearance {
    AppearanceMode mode{AppearanceMode::Dark};
    bool glassEnabled{true};
    float opacity{0.62f};
    float blurRadius{18.0f};
    float cornerRadius{22.0f};
};

struct WidgetInstance {
    std::string instanceId;
    std::string typeId;
    std::wstring monitorId{L"primary"};
    LayoutMode layoutMode{LayoutMode::Grid};
    GridPlacement grid{};
    FreePlacement free{};
    WidgetAppearance appearance{};
    float contentScale{1.0f};
    bool locked{false};
    bool selected{false};
    bool primarySelection{false};
    std::unique_ptr<IWidget> content;
};

} // namespace ws
