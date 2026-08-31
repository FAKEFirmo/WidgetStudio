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
        .monitorPixelX = info.rcMonitor.left,
        .monitorPixelY = info.rcMonitor.top,
        .monitorPixelWidth = info.rcMonitor.right - info.rcMonitor.left,
        .monitorPixelHeight = info.rcMonitor.bottom - info.rcMonitor.top,
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

std::size_t MonitorTopology::ReconcileWidgets(
    WidgetScene& scene, int gridColumns, int gridRows) const {
    const MonitorDescriptor* primary = Primary();
    if (!primary) return 0;
    std::size_t changed = 0;
    for (WidgetInstance& widget : scene.Widgets()) {
        const MonitorDescriptor* destination = Find(widget.monitorId);
        bool widgetChanged = false;
        if (!destination) {
            destination = primary;
            widget.monitorId = destination->id;
            widgetChanged = true;
        }
        const GridPlacement oldGrid = widget.grid;
        const FreePlacement oldFree = widget.free;
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
        widgetChanged = widgetChanged ||
            widget.grid.column != oldGrid.column || widget.grid.row != oldGrid.row ||
            widget.grid.columnSpan != oldGrid.columnSpan || widget.grid.rowSpan != oldGrid.rowSpan ||
            widget.free.x != oldFree.x || widget.free.y != oldFree.y ||
            widget.free.width != oldFree.width || widget.free.height != oldFree.height;
        if (widgetChanged) ++changed;
    }
    return changed;
}

} // namespace ws
