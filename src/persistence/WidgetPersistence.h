#pragma once

#include "scene/WidgetInstance.h"

#include <string>
#include <vector>

namespace ws {

// File encoding is intentionally deferred; this is the stable persistence boundary.
struct WidgetPersistenceRecord {
    std::string instanceId;
    std::string typeId;
    std::wstring monitorId;
    LayoutMode layoutMode{LayoutMode::Grid};
    GridPlacement grid{};
    FreePlacement free{};
    bool locked{false};
    float contentScale{1.0f};
    WidgetAppearance appearance{};
    WidgetState widgetState{};
};

using WidgetSceneSnapshot = std::vector<WidgetPersistenceRecord>;

} // namespace ws
