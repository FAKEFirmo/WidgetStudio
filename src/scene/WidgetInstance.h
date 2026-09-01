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

enum class SurfaceMode {
    Frosted,
    Transparent,
    Solid,
};

struct WidgetAppearance {
    AppearanceMode mode{AppearanceMode::Dark};
    SurfaceMode surface{SurfaceMode::Frosted};
    bool glassEnabled{true};
    float opacity{0.62f};
    float blurRadius{18.0f};
    float cornerRadius{24.0f};
    float innerPadding{20.0f};
    bool borderEnabled{true};
    bool shadowEnabled{true};
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
    WidgetState preservedWidgetState;
    std::unique_ptr<IWidget> content;
};

} // namespace ws
