#pragma once

#include "app/TrayController.h"
#include "common/Geometry.h"
#include "layout/GridLayout.h"
#include "rendering/Renderer.h"
#include "scene/WidgetScene.h"

#include <cstdint>
#include <optional>
#include <windows.h>

namespace ws {

class DesktopHost {
public:
    DesktopHost() = default;
    ~DesktopHost() = default;

    bool Create(HINSTANCE instance, int showCommand);
    int RunMessageLoop();

private:
    struct DragState {
        std::uint64_t widgetId{};
        PointF offset{};
    };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    bool RegisterWindowClass(HINSTANCE instance);
    void ToggleEditMode();
    void SetEditMode(bool enabled);
    void UpdateMetrics();
    void BeginDrag(std::uint64_t widgetId, PointF pointer);
    void UpdateDrag(PointF pointer);
    void EndDrag();
    void Paint();

    [[nodiscard]] PointF ClientPointFromLParam(LPARAM lParam) const noexcept;

    HINSTANCE instance_{};
    HWND hwnd_{};
    bool editMode_{true};

    Renderer renderer_{};
    GridLayout grid_{};
    GridMetrics metrics_{};
    WidgetScene scene_{};
    TrayController tray_{};
    std::optional<DragState> drag_{};
};

} // namespace ws
