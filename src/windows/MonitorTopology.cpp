#include "windows/MonitorTopology.h"

#include "scene/WidgetScene.h"

#include <algorithm>
#include <shellscalingapi.h>
#include <utility>
#include <windows.h>

namespace ws {
namespace {

BOOL CALLBACK CollectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM context) {
    auto& monitors = *reinterpret_cast<std::vector<MonitorDescriptor>*>(context);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, reinterpret_cast<MONITORINFO*>(&info))) return TRUE;

    UINT dpiX = 96;
    UINT dpiY = 96;
    if (FAILED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) dpiX = 96;
    const float pixelsToDips = 96.0f / static_cast<float>(std::max(1u, dpiX));
    monitors.push_back(MonitorDescriptor{
        .id = info.szDevice,
        .workAreaDips = RectF{
            0.0f,
            0.0f,
            static_cast<float>(info.rcWork.right - info.rcWork.left) * pixelsToDips,
            static_cast<float>(info.rcWork.bottom - info.rcWork.top) * pixelsToDips,
        },
        .pixelX = info.rcWork.left,
        .pixelY = info.rcWork.top,
        .pixelWidth = info.rcWork.right - info.rcWork.left,
        .pixelHeight = info.rcWork.bottom - info.rcWork.top,
        .dpi = dpiX,
        .primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0,
    });
    return TRUE;
}

} // namespace

bool MonitorTopology::Refresh() {
    std::vector<MonitorDescriptor> discovered;
    if (!EnumDisplayMonitors(nullptr, nullptr, CollectMonitor,
            reinterpret_cast<LPARAM>(&discovered)) || discovered.empty()) return false;
    monitors_ = std::move(discovered);
    return true;
}

const MonitorDescriptor* MonitorTopology::Find(std::wstring_view id) const noexcept {
    const auto match = std::find_if(monitors_.begin(), monitors_.end(),
        [id](const MonitorDescriptor& monitor) { return monitor.id == id; });
    return match == monitors_.end() ? nullptr : &*match;
}

const MonitorDescriptor* MonitorTopology::Primary() const noexcept {
    const auto primary = std::find_if(monitors_.begin(), monitors_.end(),
        [](const MonitorDescriptor& monitor) { return monitor.primary; });
    if (primary != monitors_.end()) return &*primary;
    return monitors_.empty() ? nullptr : &monitors_.front();
}

std::size_t MonitorTopology::MigrateMissingWidgets(
    WidgetScene& scene, int gridColumns, int gridRows) const {
    const MonitorDescriptor* destination = Primary();
    if (!destination) return 0;
    std::size_t migrated = 0;
    for (WidgetInstance& widget : scene.Widgets()) {
        if (Find(widget.monitorId)) continue;
        widget.monitorId = destination->id;
        widget.grid.columnSpan = std::clamp(widget.grid.columnSpan, 1, std::max(1, gridColumns));
        widget.grid.rowSpan = std::clamp(widget.grid.rowSpan, 1, std::max(1, gridRows));
        widget.grid.column = std::clamp(widget.grid.column, 0,
            std::max(0, gridColumns - widget.grid.columnSpan));
        widget.grid.row = std::clamp(widget.grid.row, 0,
            std::max(0, gridRows - widget.grid.rowSpan));
        widget.free.width = std::clamp(widget.free.width, 1.0f,
            std::max(1.0f, destination->workAreaDips.width));
        widget.free.height = std::clamp(widget.free.height, 1.0f,
            std::max(1.0f, destination->workAreaDips.height));
        widget.free.x = std::clamp(widget.free.x, 0.0f,
            destination->workAreaDips.width - widget.free.width);
        widget.free.y = std::clamp(widget.free.y, 0.0f,
            destination->workAreaDips.height - widget.free.height);
        ++migrated;
    }
    return migrated;
}

} // namespace ws
