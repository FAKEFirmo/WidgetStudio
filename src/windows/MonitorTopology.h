#pragma once

#include "common/Geometry.h"

#include <string>
#include <string_view>
#include <vector>

namespace ws {

class WidgetScene;

struct MonitorDescriptor {
    std::wstring id;
    RectF workAreaDips{};
    unsigned int dpi{96};
    bool primary{false};
};

class MonitorTopology {
public:
    bool Refresh();
    [[nodiscard]] const std::vector<MonitorDescriptor>& Monitors() const noexcept { return monitors_; }
    [[nodiscard]] const MonitorDescriptor* Find(std::wstring_view id) const noexcept;
    [[nodiscard]] const MonitorDescriptor* Primary() const noexcept;
    std::size_t MigrateMissingWidgets(WidgetScene& scene, int gridColumns, int gridRows) const noexcept;

private:
    std::vector<MonitorDescriptor> monitors_;
};

} // namespace ws
