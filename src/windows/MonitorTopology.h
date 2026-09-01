#pragma once

#include "common/Geometry.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ws {

class WidgetScene;

struct MonitorDescriptor {
    std::wstring id;
    RectF monitorBoundsDips{};
    RectF workAreaDips{};
    RectF workAreaOnMonitorDips{};
    int pixelX{};
    int pixelY{};
    int pixelWidth{};
    int pixelHeight{};
    int monitorPixelX{};
    int monitorPixelY{};
    int monitorPixelWidth{};
    int monitorPixelHeight{};
    int virtualPixelX{};
    int virtualPixelY{};
    int virtualPixelWidth{};
    int virtualPixelHeight{};
    unsigned int dpi{96};
    bool primary{false};
};

class MonitorTopology {
public:
    MonitorTopology() = default;
    explicit MonitorTopology(std::vector<MonitorDescriptor> monitors)
        : monitors_(std::move(monitors)) {}
    bool Refresh();
    [[nodiscard]] const std::vector<MonitorDescriptor>& Monitors() const noexcept { return monitors_; }
    [[nodiscard]] const MonitorDescriptor* Find(std::wstring_view id) const noexcept;
    [[nodiscard]] const MonitorDescriptor* Primary() const noexcept;
    std::size_t ReconcileWidgets(WidgetScene& scene, int gridColumns, int gridRows) const;

private:
    std::vector<MonitorDescriptor> monitors_;
};

} // namespace ws
