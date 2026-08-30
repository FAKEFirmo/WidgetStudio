#pragma once

#include "app/TrayController.h"
#include "app/WidgetLibraryWindow.h"
#include "app/WidgetStudioWindow.h"
#include "common/Geometry.h"
#include "layout/GridLayout.h"
#include "persistence/SceneStore.h"
#include "scene/WidgetScene.h"
#include "widgets/WidgetRegistry.h"
#include "windows/MonitorTopology.h"
#include "windows/StartupShortcutService.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <windows.h>

namespace ws {

class MediaSessionService;
class WidgetWindow;

class DesktopHost {
public:
    DesktopHost(const WidgetRegistry& registry, std::shared_ptr<MediaSessionService> mediaSession);
    ~DesktopHost();

    bool Create(HINSTANCE instance, int showCommand);
    int RunMessageLoop();

private:
    friend class WidgetWindow;
    struct WidgetActionHit {
        std::string instanceId;
        std::string actionId;
    };

    struct DragState {
        std::string widgetId;
        PointF offset{};
        HWND captureWindow{};
        GridMetrics metrics{};
        RectF bounds{};
        bool moved{false};
    };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    bool RegisterWindowClass(HINSTANCE instance);
    void ToggleEditMode();
    void SetEditMode(bool enabled);
    void BeginDrag(std::string_view widgetId, PointF pointer, HWND captureWindow,
        const GridMetrics& metrics, RectF bounds);
    void UpdateDrag(PointF pointer);
    void EndDrag();
    void OpenWidgetLibrary();
    void OpenWidgetStudio();
    void ToggleLaunchAtLogin();
    void CreateWidget(std::string_view typeId, bool persist = true);
    void DeleteSelectedWidgets();
    void DuplicatePrimaryWidget();
    void TogglePrimaryWidgetLock();
    void LockAllWidgets();
    [[nodiscard]] SceneLoadStatus LoadScene();
    void SaveScene();
    void ScheduleNextWidgetUpdate();
    void InvalidateDesktop(bool reloadWallpaper = false);
    void SynchronizeWidgetWindows();
    void RefreshMonitorConfiguration();
    void ActivateMonitor(std::wstring_view monitorId, const GridMetrics& metrics, RectF bounds);
    [[nodiscard]] std::wstring ActiveMonitorId() const;
    [[nodiscard]] std::optional<WidgetActionHit> HitTestWidgetAction(
        const WidgetWindow& window, PointF localPoint) const;
    LRESULT HandleWidgetNcHitTest(WidgetWindow& window, WPARAM wParam, LPARAM lParam);
    void HandleWidgetLeftDown(WidgetWindow& window, WPARAM wParam, LPARAM lParam);
    void HandleWidgetMouseMove(WidgetWindow& window, WPARAM wParam, LPARAM lParam);
    bool HandleWidgetKeyDown(WPARAM key);
    [[nodiscard]] GridLayout& Grid() noexcept { return grid_; }
    [[nodiscard]] WidgetScene& Scene() noexcept { return scene_; }
    [[nodiscard]] bool EditMode() const noexcept { return editMode_; }

    HINSTANCE instance_{};
    HWND hwnd_{};
    UINT taskbarCreatedMessage_{};
    bool editMode_{false};
    bool hotkeyRegistered_{false};
    std::wstring activeMonitorId_;
    GridMetrics activeMetrics_{};
    RectF activeBounds_{};

    GridLayout grid_{};
    GridMetrics metrics_{};
    const WidgetRegistry& registry_;
    std::shared_ptr<MediaSessionService> mediaSession_;
    WidgetScene scene_;
    SceneStore sceneStore_;
    WidgetSceneSnapshot unrestoredRecords_;
    bool persistenceErrorShown_{false};
    TrayController tray_{};
    std::vector<std::unique_ptr<WidgetWindow>> widgetWindows_;
    MonitorTopology monitorTopology_{};
    StartupShortcutService startupShortcut_{};
    WidgetLibraryWindow library_{};
    WidgetStudioWindow studio_{};
    std::optional<DragState> drag_{};
};

} // namespace ws
