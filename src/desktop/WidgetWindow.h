#pragma once

#include "common/Geometry.h"
#include "desktop/DesktopBackendController.h"
#include "layout/GridLayout.h"
#include "rendering/Renderer.h"
#include "windows/MonitorTopology.h"

#include <string>
#include <string_view>
#include <windows.h>

namespace ws {

class DesktopHost;

class WidgetWindow {
public:
    WidgetWindow() = default;
    ~WidgetWindow();
    WidgetWindow(const WidgetWindow&) = delete;
    WidgetWindow& operator=(const WidgetWindow&) = delete;

    bool Create(DesktopHost& host, HINSTANCE instance, std::string instanceId,
        const MonitorDescriptor& monitor);
    void Close() noexcept;
    bool UpdatePlacement(const MonitorDescriptor& monitor);
    void SetEditMode(bool enabled);
    void Invalidate();
    void Reattach();
    void SetZOrderAfter(HWND insertAfter);

    [[nodiscard]] HWND Window() const noexcept { return hwnd_; }
    [[nodiscard]] UINT Dpi() const noexcept { return dpi_; }
    [[nodiscard]] const std::string& InstanceId() const noexcept { return instanceId_; }
    [[nodiscard]] const std::wstring& MonitorId() const noexcept { return monitorId_; }
    [[nodiscard]] const GridMetrics& Metrics() const noexcept { return metrics_; }
    [[nodiscard]] RectF WidgetBounds() const noexcept { return widgetBounds_; }
    [[nodiscard]] RectF WindowBounds() const noexcept { return windowBounds_; }
    [[nodiscard]] RectF WidgetBoundsInWindow() const noexcept { return widgetBoundsInWindow_; }
    [[nodiscard]] RectF MonitorBounds() const noexcept {
        return {0.0f, 0.0f, monitorSize_.width, monitorSize_.height};
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void Paint();

    DesktopHost* host_{};
    HINSTANCE instance_{};
    HWND hwnd_{};
    UINT dpi_{96};
    bool renderFailureLogged_{false};
    std::string instanceId_;
    std::wstring monitorId_;
    GridMetrics metrics_{};
    RectF widgetBounds_{};
    RectF windowBounds_{};
    RectF widgetBoundsInWindow_{};
    RectF wallpaperBounds_{};
    SizeF wallpaperDesktopSize_{};
    SizeF monitorSize_{};
    Renderer renderer_{};
    DesktopBackendController backend_{};
};

} // namespace ws
