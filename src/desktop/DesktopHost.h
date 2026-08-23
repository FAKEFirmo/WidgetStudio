#pragma once

#include "app/TrayController.h"
#include "app/WidgetLibraryWindow.h"
#include "common/Geometry.h"
#include "layout/GridLayout.h"
#include "persistence/SceneStore.h"
#include "rendering/Renderer.h"
#include "scene/WidgetScene.h"
#include "widgets/WidgetRegistry.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <windows.h>

namespace ws {

class DesktopHost {
public:
    explicit DesktopHost(const WidgetRegistry& registry);
    ~DesktopHost();

    bool Create(HINSTANCE instance, int showCommand);
    int RunMessageLoop();

private:
    struct WidgetActionHit {
        std::string instanceId;
        std::string actionId;
    };

    struct DragState {
        std::string widgetId;
        PointF offset{};
        bool moved{false};
    };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    bool RegisterWindowClass(HINSTANCE instance);
    void ToggleEditMode();
    void SetEditMode(bool enabled);
    void UpdateMetrics();
    void BeginDrag(std::string_view widgetId, PointF pointer);
    void UpdateDrag(PointF pointer);
    void EndDrag();
    void Paint();
    void OpenWidgetLibrary();
    void CreateWidget(std::string_view typeId, bool persist = true);
    void DeleteSelectedWidgets();
    void DuplicatePrimaryWidget();
    void TogglePrimaryWidgetLock();
    [[nodiscard]] SceneLoadStatus LoadScene();
    void SaveScene();
    void ScheduleNextWidgetUpdate();
    [[nodiscard]] std::wstring ActiveMonitorId() const;

    [[nodiscard]] PointF ClientPointFromLParam(LPARAM lParam) const noexcept;
    [[nodiscard]] RectF ClientBounds() const noexcept;
    [[nodiscard]] std::optional<WidgetActionHit> HitTestWidgetAction(PointF point) const;

    HINSTANCE instance_{};
    HWND hwnd_{};
    UINT dpi_{96};
    bool editMode_{true};

    Renderer renderer_{};
    GridLayout grid_{};
    GridMetrics metrics_{};
    const WidgetRegistry& registry_;
    WidgetScene scene_;
    SceneStore sceneStore_;
    WidgetSceneSnapshot unrestoredRecords_;
    bool persistenceErrorShown_{false};
    TrayController tray_{};
    WidgetLibraryWindow library_{};
    std::optional<DragState> drag_{};
};

} // namespace ws
