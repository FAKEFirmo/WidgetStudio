#include "desktop/DesktopHost.h"

#include "desktop/WidgetWindow.h"
#include "layout/OuterLayout.h"
#include "rendering/WidgetVisualStyle.h"
#include "windows/MediaSessionService.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <limits>
#include <utility>
#include <windowsx.h>

namespace ws {
namespace {

constexpr wchar_t kWindowClassName[] = L"WidgetStudioHostWindow";
constexpr wchar_t kWindowTitle[] = L"WidgetStudio";
constexpr int kHotkeyToggleEdit = 1;
constexpr UINT_PTR kWidgetUpdateTimer = 2;
constexpr UINT kMediaSessionChangedMessage = WM_APP + 12;
constexpr UINT kMonitorRefreshMessage = WM_APP + 13;

} // namespace

DesktopHost::DesktopHost(const WidgetRegistry& registry, std::shared_ptr<MediaSessionService> mediaSession)
    : registry_(registry), renderingResources_(std::make_shared<RenderingResources>()),
      wallpaperCache_(std::make_shared<WallpaperCache>()),
      mediaSession_(std::move(mediaSession)), scene_(registry),
      sceneStore_(SceneStore::DefaultConfigPath()) {
    scene_.SetGridDimensions(grid_.Columns(), grid_.Rows());
}

DesktopHost::~DesktopHost() {
    if (mediaSession_) mediaSession_->SetChangedCallback({});
    if (hwnd_ && IsWindow(hwnd_)) {
        DestroyWindow(hwnd_);
    }
}

bool DesktopHost::RegisterWindowClass(HINSTANCE instance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = kWindowClassName;
    return RegisterClassExW(&wc) != 0;
}

bool DesktopHost::Create(HINSTANCE instance, int showCommand) {
    static_cast<void>(showCommand);
    instance_ = instance;
    if (!RegisterWindowClass(instance_)) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kWindowClassName,
        kWindowTitle,
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        instance_,
        this);

    if (!hwnd_) {
        return false;
    }
    taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");
    if (mediaSession_) {
        const HWND notificationWindow = hwnd_;
        mediaSession_->SetChangedCallback([notificationWindow] {
            PostMessageW(notificationWindow, kMediaSessionChangedMessage, 0, 0);
        });
    }

    if (!tray_.Initialize(hwnd_)) {
        tray_.Shutdown();
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }
    hotkeyRegistered_ = RegisterHotKey(
        hwnd_, kHotkeyToggleEdit, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'W') != FALSE;

    if (!monitorTopology_.Refresh() || !monitorTopology_.Primary()) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }
    const MonitorDescriptor& primary = *monitorTopology_.Primary();
    activeMonitorId_ = primary.id;
    activeBounds_ = primary.workAreaDips;
    activeMetrics_ = grid_.Calculate({primary.workAreaDips.width, primary.workAreaDips.height});
    metrics_ = activeMetrics_;
    const SceneLoadStatus loadStatus = LoadScene();
    if (loadStatus != SceneLoadStatus::Loaded) {
        CreateWidget("clock", loadStatus == SceneLoadStatus::Missing);
    }
    if (monitorTopology_.ReconcileWidgets(
            scene_, grid_.Columns(), grid_.Rows()) > 0) SaveScene();
    SynchronizeWidgetWindows();
    ScheduleNextWidgetUpdate();
    return true;
}

int DesktopHost::RunMessageLoop() {
    MSG message{};
    while (true) {
        const BOOL status = GetMessageW(&message, nullptr, 0, 0);
        if (status == -1) {
            return 1;
        }
        if (status == 0) {
            return static_cast<int>(message.wParam);
        }
        if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE && editMode_) {
            SetEditMode(false);
            continue;
        }
        if ((library_.Window() && IsDialogMessageW(library_.Window(), &message)) ||
            (studio_.Window() && IsDialogMessageW(studio_.Window(), &message))) {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

LRESULT CALLBACK DesktopHost::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    DesktopHost* self = nullptr;

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<DesktopHost*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<DesktopHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        const LRESULT result = self->HandleMessage(message, wParam, lParam);
        if (message == WM_NCDESTROY) {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            self->hwnd_ = nullptr;
        }
        return result;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

std::optional<DesktopHost::WidgetActionHit> DesktopHost::HitTestWidgetAction(
    const WidgetWindow& window, PointF localPoint) const {
    const WidgetInstance* widget = scene_.Find(window.InstanceId());
    if (!widget || !widget->content) return std::nullopt;
    const RectF outer = window.WidgetBoundsInWindow();
    const auto action = widget->content->HitTestAction(WidgetHitTestContext{
        .point = localPoint,
        .bounds = WidgetVisualStyle::ContentBounds(outer),
        .contentScale = widget->contentScale,
    });
    if (!action) return std::nullopt;
    return WidgetActionHit{widget->instanceId, *action};
}

void DesktopHost::ToggleEditMode() {
    SetEditMode(!editMode_);
}

void DesktopHost::SetEditMode(bool enabled) {
    editMode_ = enabled;
    if (!editMode_) {
        EndDrag();
    }
    for (const auto& window : widgetWindows_) window->SetEditMode(editMode_);
    InvalidateDesktop();
}

void DesktopHost::BeginDrag(std::string_view widgetId, PointF pointer, HWND captureWindow,
    const GridMetrics& metrics, RectF bounds) {
    WidgetInstance* widget = scene_.Find(widgetId);
    if (!widget || widget->locked) return;

    const RectF rect = OuterLayout::RectFor(*widget, grid_, metrics);
    drag_ = DragState{
        .widgetId = std::string(widgetId),
        .offset = PointF{pointer.x - rect.x, pointer.y - rect.y},
        .captureWindow = captureWindow,
        .metrics = metrics,
        .bounds = bounds,
        .moved = false,
    };
    SetCapture(captureWindow);
}

void DesktopHost::OpenWidgetLibrary() {
    library_.Open(nullptr, instance_, registry_, [this](std::string typeId) { CreateWidget(typeId); });
}

void DesktopHost::OpenWidgetStudio() {
    const std::filesystem::path assetDirectory = SceneStore::DefaultImageDirectory();
    const GridMetrics studioMetrics = activeBounds_.width > 0.0f ? activeMetrics_ : metrics_;
    const RectF studioBounds = activeBounds_;
    if (!studio_.Open(nullptr, instance_, scene_, grid_, studioMetrics, studioBounds, assetDirectory,
            ActiveMonitorId(), monitorTopology_.Monitors(), wallpaperCache_, renderingResources_, [this] {
        const auto primary = scene_.PrimarySelection();
        const WidgetInstance* widget = primary ? scene_.Find(*primary) : nullptr;
        const MonitorDescriptor* monitor = widget ? monitorTopology_.Find(widget->monitorId) : nullptr;
        if (monitor) {
            ActivateMonitor(monitor->id,
                grid_.Calculate({monitor->workAreaDips.width, monitor->workAreaDips.height}),
                monitor->workAreaDips);
        }
        SaveScene();
        ScheduleNextWidgetUpdate();
        SynchronizeWidgetWindows();
        InvalidateDesktop();
    }, [this] {
        SynchronizeWidgetWindows();
        InvalidateDesktop();
    }, [this] { OpenWidgetLibrary(); })) {
        MessageBoxW(hwnd_, L"Widget Studio could not open its settings window.",
            L"Widget Studio", MB_OK | MB_ICONERROR);
    }
}

void DesktopHost::ToggleLaunchAtLogin() {
    std::wstring error;
    if (!startupShortcut_.SetEnabled(!startupShortcut_.IsEnabled(), error)) {
        MessageBoxW(hwnd_, error.c_str(), L"Widget Studio", MB_OK | MB_ICONERROR);
    }
}

void DesktopHost::CreateWidget(std::string_view typeId, bool persist) {
    if (!scene_.CreateWidget(typeId, ActiveMonitorId())) return;
    SetEditMode(true);
    if (persist) SaveScene();
    ScheduleNextWidgetUpdate();
    SynchronizeWidgetWindows();
    InvalidateDesktop();
    studio_.Refresh();
}

std::wstring DesktopHost::ActiveMonitorId() const {
    if (!activeMonitorId_.empty()) return activeMonitorId_;
    const MonitorDescriptor* primary = monitorTopology_.Primary();
    return primary ? primary->id : L"primary";
}

void DesktopHost::ActivateMonitor(
    std::wstring_view monitorId, const GridMetrics& metrics, RectF bounds) {
    activeMonitorId_ = monitorId;
    activeMetrics_ = metrics;
    activeBounds_ = bounds;
    studio_.UpdateLayoutContext(metrics, bounds, activeMonitorId_);
}

void DesktopHost::InvalidateDesktop(bool reloadWallpaper) {
    if (reloadWallpaper && wallpaperCache_) static_cast<void>(wallpaperCache_->Reload());
    for (const auto& window : widgetWindows_) window->Invalidate();
}

void DesktopHost::SynchronizeWidgetWindows() {
    std::erase_if(widgetWindows_, [this](const auto& window) {
        return !window->Window() || !IsWindow(window->Window()) ||
            scene_.Find(window->InstanceId()) == nullptr;
    });
    bool creationFailed = false;
    for (const WidgetInstance& widget : scene_.Widgets()) {
        const MonitorDescriptor* monitor = monitorTopology_.Find(widget.monitorId);
        if (!monitor) monitor = monitorTopology_.Primary();
        if (!monitor) continue;
        const auto existing = std::find_if(widgetWindows_.begin(), widgetWindows_.end(),
            [&widget](const auto& window) { return window->InstanceId() == widget.instanceId; });
        if (existing == widgetWindows_.end()) {
            auto window = std::make_unique<WidgetWindow>();
            if (window->Create(*this, instance_, widget.instanceId, *monitor)) {
                widgetWindows_.push_back(std::move(window));
            } else {
                creationFailed = true;
            }
        } else {
            if (!(*existing)->UpdatePlacement(*monitor)) creationFailed = true;
        }
    }
    if (creationFailed && !widgetWindowErrorShown_) {
        MessageBoxW(hwnd_,
            L"Widget Studio could not create or position one or more desktop widget windows. "
            L"The scene remains saved and WidgetStudio will retry after a display or Explorer refresh.",
            L"Widget Studio", MB_OK | MB_ICONWARNING);
        widgetWindowErrorShown_ = true;
    } else if (!creationFailed) {
        widgetWindowErrorShown_ = false;
    }

    HWND previous = HWND_BOTTOM;
    HWND previousParent = nullptr;
    // SetWindowPos(window, sibling) places window behind sibling. Walk the
    // scene from frontmost to backmost while inserting at the bottom so the
    // final native order matches rendering/hit testing (last scene item wins).
    for (auto sceneItem = scene_.Widgets().rbegin(); sceneItem != scene_.Widgets().rend(); ++sceneItem) {
        const WidgetInstance& widget = *sceneItem;
        const auto found = std::find_if(widgetWindows_.begin(), widgetWindows_.end(),
            [&widget](const auto& window) { return window->InstanceId() == widget.instanceId; });
        if (found == widgetWindows_.end()) continue;
        const HWND parent = GetParent((*found)->Window());
        if (parent != previousParent) previous = HWND_BOTTOM;
        (*found)->SetZOrderAfter(previous);
        previous = (*found)->Window();
        previousParent = parent;
    }
}

void DesktopHost::RefreshMonitorConfiguration() {
    if (!monitorTopology_.Refresh()) return;
    if (monitorTopology_.ReconcileWidgets(
            scene_, grid_.Columns(), grid_.Rows()) > 0) SaveScene();
    const MonitorDescriptor* active = monitorTopology_.Find(activeMonitorId_);
    if (!active) active = monitorTopology_.Primary();
    if (active) {
        ActivateMonitor(active->id,
            grid_.Calculate({active->workAreaDips.width, active->workAreaDips.height}),
            active->workAreaDips);
        metrics_ = activeMetrics_;
    }
    SynchronizeWidgetWindows();
    studio_.UpdateMonitors(monitorTopology_.Monitors());
    InvalidateDesktop();
    studio_.Refresh();
}

void DesktopHost::RequestMonitorRefresh() {
    if (!hwnd_ || monitorRefreshPending_) return;
    monitorRefreshPending_ = PostMessageW(hwnd_, kMonitorRefreshMessage, 0, 0) != FALSE;
}

void DesktopHost::DeleteSelectedWidgets() {
    EndDrag();
    if (scene_.RemoveSelectedWidgets() > 0) {
        SaveScene();
        ScheduleNextWidgetUpdate();
        SynchronizeWidgetWindows();
        InvalidateDesktop();
        studio_.Refresh();
    }
}

void DesktopHost::DuplicatePrimaryWidget() {
    const auto primary = scene_.PrimarySelection();
    if (primary && scene_.DuplicateWidget(*primary)) {
        SaveScene();
        ScheduleNextWidgetUpdate();
        SynchronizeWidgetWindows();
        InvalidateDesktop();
        studio_.Refresh();
    }
}

void DesktopHost::TogglePrimaryWidgetLock() {
    const auto primary = scene_.PrimarySelection();
    if (!primary) return;
    const WidgetInstance* widget = scene_.Find(*primary);
    if (!widget) return;
    const bool lock = !widget->locked;
    scene_.SetWidgetLocked(*primary, lock);
    if (lock) EndDrag();
    SaveScene();
    InvalidateDesktop();
    studio_.Refresh();
}

void DesktopHost::LockAllWidgets() {
    EndDrag();
    bool changed = false;
    for (WidgetInstance& widget : scene_.Widgets()) {
        if (!widget.locked) {
            widget.locked = true;
            changed = true;
        }
    }
    if (!changed) return;
    SaveScene();
    InvalidateDesktop();
    studio_.Refresh();
}

SceneLoadStatus DesktopHost::LoadScene() {
    const SceneLoadResult result = sceneStore_.Load();
    if (result.status == SceneLoadStatus::Missing) return result.status;
    if (result.status != SceneLoadStatus::Loaded) {
        const std::wstring message = L"Widget Studio could not load the saved scene. The existing file was left intact.\n\n" +
            result.message;
        MessageBoxW(hwnd_, message.c_str(), L"Widget Studio", MB_OK | MB_ICONWARNING);
        return result.status;
    }

    for (const auto& record : result.snapshot) {
        if (!scene_.RestoreWidget(record)) unrestoredRecords_.push_back(record);
    }
    return SceneLoadStatus::Loaded;
}

void DesktopHost::SaveScene() {
    WidgetSceneSnapshot snapshot = scene_.Snapshot();
    snapshot.insert(snapshot.end(), unrestoredRecords_.begin(), unrestoredRecords_.end());
    std::wstring error;
    if (sceneStore_.Save(snapshot, error)) {
        persistenceErrorShown_ = false;
        return;
    }
    if (!persistenceErrorShown_) {
        const std::wstring message = L"Widget Studio could not save the scene.\n\n" + error;
        MessageBoxW(hwnd_, message.c_str(), L"Widget Studio", MB_OK | MB_ICONWARNING);
        persistenceErrorShown_ = true;
    }
}

void DesktopHost::ScheduleNextWidgetUpdate() {
    if (!hwnd_) return;
    KillTimer(hwnd_, kWidgetUpdateTimer);
    scheduledWidgetUpdates_.clear();
    std::optional<std::chrono::system_clock::time_point> nextUpdate;
    for (const auto& widget : scene_.Widgets()) {
        if (!widget.content) continue;
        const auto candidate = widget.content->NextUpdateTime();
        if (!candidate) continue;
        scheduledWidgetUpdates_.insert_or_assign(widget.instanceId, *candidate);
        if (!nextUpdate || *candidate < *nextUpdate) nextUpdate = candidate;
    }
    if (!nextUpdate) return;
    const auto now = std::chrono::system_clock::now();
    const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(*nextUpdate - now).count();
    const auto delay = static_cast<UINT>(std::clamp<long long>(
        remaining, USER_TIMER_MINIMUM, static_cast<long long>(std::numeric_limits<UINT>::max())));
    SetTimer(hwnd_, kWidgetUpdateTimer, delay, nullptr);
}

bool DesktopHost::InvalidateDueWidgets() {
    const auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(4);
    bool invalidated = false;
    for (const auto& [instanceId, due] : scheduledWidgetUpdates_) {
        if (due > deadline) continue;
        const auto window = std::find_if(widgetWindows_.begin(), widgetWindows_.end(),
            [&instanceId](const auto& item) { return item->InstanceId() == instanceId; });
        if (window != widgetWindows_.end()) {
            (*window)->Invalidate();
            invalidated = true;
        }
    }
    return invalidated;
}

void DesktopHost::InvalidateInteractiveWidgets() {
    for (const auto& window : widgetWindows_) {
        const WidgetInstance* widget = scene_.Find(window->InstanceId());
        const WidgetDescriptor* descriptor = widget ? registry_.Find(widget->typeId) : nullptr;
        if (descriptor && HasCapability(descriptor->capabilities, WidgetCapability::Interactive)) {
            window->Invalidate();
        }
    }
}

void DesktopHost::UpdateDrag(PointF pointer) {
    if (!drag_) return;

    WidgetInstance* widget = scene_.Find(drag_->widgetId);
    if (!widget || widget->locked) return;

    if (widget->layoutMode == LayoutMode::Grid) {
        const GridPlacement moved = grid_.MoveToPoint(
            widget->grid, pointer, drag_->offset, drag_->metrics);
        if (moved.column != widget->grid.column || moved.row != widget->grid.row) {
            widget->grid = moved;
            drag_->moved = true;
        }
    } else {
        const FreePlacement moved = OuterLayout::MoveFreeToPoint(
            widget->free, pointer, drag_->offset, drag_->bounds);
        if (moved.x != widget->free.x || moved.y != widget->free.y) {
            widget->free = moved;
            drag_->moved = true;
        }
    }
    SynchronizeWidgetWindows();
    InvalidateDesktop();
}

void DesktopHost::EndDrag() {
    if (drag_) {
        const bool moved = drag_->moved;
        const HWND captureWindow = drag_->captureWindow;
        drag_.reset();
        if (GetCapture() == captureWindow) {
            ReleaseCapture();
        }
        if (moved) SaveScene();
        if (moved) studio_.Refresh();
    }
}

LRESULT DesktopHost::HandleWidgetNcHitTest(
    WidgetWindow& window, WPARAM, LPARAM lParam) {
    if (editMode_) return HTCLIENT;
    POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (!ScreenToClient(window.Window(), &point)) return HTTRANSPARENT;
    const float pixelsToDips = 96.0f / static_cast<float>(std::max(1u, window.Dpi()));
    const PointF clientPoint{static_cast<float>(point.x) * pixelsToDips,
        static_cast<float>(point.y) * pixelsToDips};
    if (HitTestWidgetAction(window, clientPoint)) return HTCLIENT;
    const WidgetInstance* widget = scene_.Find(window.InstanceId());
    const WidgetDescriptor* descriptor = widget ? registry_.Find(widget->typeId) : nullptr;
    return descriptor && !HasCapability(descriptor->capabilities, WidgetCapability::PassiveClickThrough)
        ? HTCLIENT : HTTRANSPARENT;
}

void DesktopHost::HandleWidgetLeftDown(
    WidgetWindow& window, WPARAM, LPARAM lParam) {
    ActivateMonitor(window.MonitorId(), window.Metrics(), window.MonitorBounds());
    const float pixelsToDips = 96.0f / static_cast<float>(std::max(1u, window.Dpi()));
    const PointF localPoint{static_cast<float>(GET_X_LPARAM(lParam)) * pixelsToDips,
        static_cast<float>(GET_Y_LPARAM(lParam)) * pixelsToDips};
    if (const auto action = HitTestWidgetAction(window, localPoint)) {
        WidgetInstance* widget = scene_.Find(action->instanceId);
        if (widget && widget->content) static_cast<void>(widget->content->InvokeAction(action->actionId));
        ScheduleNextWidgetUpdate();
        window.Invalidate();
        return;
    }
    if (!editMode_) return;
    SetFocus(window.Window());
    scene_.Select(window.InstanceId(), (GetKeyState(VK_SHIFT) & 0x8000) != 0);
    const WidgetInstance* selected = scene_.Find(window.InstanceId());
    if (selected && selected->selected) {
        const PointF scenePoint{window.WindowBounds().x + localPoint.x,
            window.WindowBounds().y + localPoint.y};
        BeginDrag(window.InstanceId(), scenePoint, window.Window(),
            window.Metrics(), window.MonitorBounds());
    }
    InvalidateDesktop();
    studio_.Refresh();
}

void DesktopHost::HandleWidgetMouseMove(
    WidgetWindow& window, WPARAM wParam, LPARAM lParam) {
    if (editMode_ && drag_ && drag_->captureWindow == window.Window() && (wParam & MK_LBUTTON)) {
        const float pixelsToDips = 96.0f / static_cast<float>(std::max(1u, window.Dpi()));
        const PointF pointer{window.WindowBounds().x + static_cast<float>(GET_X_LPARAM(lParam)) * pixelsToDips,
            window.WindowBounds().y + static_cast<float>(GET_Y_LPARAM(lParam)) * pixelsToDips};
        UpdateDrag(pointer);
    } else if (drag_ && drag_->captureWindow == window.Window() && !(wParam & MK_LBUTTON)) {
        EndDrag();
    }
}

bool DesktopHost::HandleWidgetKeyDown(WPARAM key) {
    if (key == VK_ESCAPE && editMode_) { SetEditMode(false); return true; }
    if (editMode_ && key == VK_DELETE) { DeleteSelectedWidgets(); return true; }
    if (editMode_ && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
        if (key == 'D') { DuplicatePrimaryWidget(); return true; }
        if (key == 'L') { TogglePrimaryWidgetLock(); return true; }
    }
    return false;
}

LRESULT DesktopHost::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    if (taskbarCreatedMessage_ != 0 && message == taskbarCreatedMessage_) {
        tray_.RestoreAfterExplorerRestart();
        for (const auto& window : widgetWindows_) {
            window->Reattach();
            window->SetEditMode(editMode_);
        }
        SynchronizeWidgetWindows();
        InvalidateDesktop();
        return 0;
    }
    switch (message) {
    case WM_SETTINGCHANGE:
        if (wParam == SPI_SETWORKAREA) RefreshMonitorConfiguration();
        InvalidateDesktop(true);
        studio_.InvalidatePreview();
        ScheduleNextWidgetUpdate();
        return 0;

    case WM_DISPLAYCHANGE:
        RefreshMonitorConfiguration();
        return 0;

    case kMonitorRefreshMessage:
        monitorRefreshPending_ = false;
        RefreshMonitorConfiguration();
        return 0;

    case WM_TIMECHANGE:
        ScheduleNextWidgetUpdate();
        InvalidateDesktop();
        studio_.InvalidatePreview();
        return 0;

    case WM_TIMER:
        if (wParam == kWidgetUpdateTimer) {
            const bool invalidated = InvalidateDueWidgets();
            ScheduleNextWidgetUpdate();
            if (invalidated) studio_.InvalidatePreview();
            return 0;
        }
        break;

    case kMediaSessionChangedMessage:
        ScheduleNextWidgetUpdate();
        InvalidateInteractiveWidgets();
        studio_.InvalidatePreview();
        return 0;

    case WM_PAINT:
        ValidateRect(hwnd_, nullptr);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_KEYDOWN:
        if (HandleWidgetKeyDown(wParam)) return 0;
        break;

    case WM_HOTKEY:
        if (wParam == kHotkeyToggleEdit) {
            ToggleEditMode();
            return 0;
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case TrayController::kCommandToggleEdit:
            ToggleEditMode();
            return 0;
        case TrayController::kCommandAddWidget:
            OpenWidgetLibrary();
            return 0;
        case TrayController::kCommandOpenStudio:
            OpenWidgetStudio();
            return 0;
        case TrayController::kCommandLockAll:
            LockAllWidgets();
            return 0;
        case TrayController::kCommandToggleLaunchAtLogin:
            ToggleLaunchAtLogin();
            return 0;
        case TrayController::kCommandExit:
            DestroyWindow(hwnd_);
            return 0;
        default:
            break;
        }
        break;

    case TrayController::kTrayCallbackMessage:
        switch (LOWORD(lParam)) {
        case WM_CONTEXTMENU:
        case WM_RBUTTONUP:
            tray_.ShowContextMenu(editMode_, startupShortcut_.IsEnabled());
            return 0;
        case WM_LBUTTONDBLCLK:
            ToggleEditMode();
            return 0;
        default:
            break;
        }
        break;

    case WM_DESTROY:
        if (mediaSession_) mediaSession_->SetChangedCallback({});
        studio_.Close();
        widgetWindows_.clear();
        KillTimer(hwnd_, kWidgetUpdateTimer);
        if (hotkeyRegistered_) UnregisterHotKey(hwnd_, kHotkeyToggleEdit);
        hotkeyRegistered_ = false;
        tray_.Shutdown();
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

} // namespace ws
