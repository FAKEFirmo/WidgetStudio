#pragma once

#include "common/Geometry.h"
#include "desktop/DesktopBackendController.h"
#include "layout/GridLayout.h"
#include "rendering/Renderer.h"
#include "windows/MonitorTopology.h"

#include <string>
#include <windows.h>

namespace ws {

class DesktopHost;

class DesktopSurface {
public:
    DesktopSurface() = default;
    ~DesktopSurface();
    DesktopSurface(const DesktopSurface&) = delete;
    DesktopSurface& operator=(const DesktopSurface&) = delete;

    bool Create(DesktopHost& host, HINSTANCE instance, const MonitorDescriptor& monitor);
    void Close() noexcept;
    void Invalidate(bool reloadWallpaper = false);

    [[nodiscard]] HWND Window() const noexcept { return hwnd_; }
    [[nodiscard]] UINT Dpi() const noexcept { return dpi_; }
    [[nodiscard]] const GridMetrics& Metrics() const noexcept { return metrics_; }
    [[nodiscard]] RectF Bounds() const noexcept;
    [[nodiscard]] const std::wstring& MonitorId() const noexcept { return monitorId_; }
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void UpdateMetrics();
    void Paint();

    DesktopHost* host_{};
    HINSTANCE instance_{};
    HWND hwnd_{};
    UINT dpi_{96};
    std::wstring monitorId_;
    GridLayout* grid_{};
    GridMetrics metrics_{};
    Renderer renderer_{};
    DesktopBackendController backend_{};
};

} // namespace ws
