#include "desktop/DesktopHost.h"

#include "desktop/DesktopSurface.h"
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

} // namespace

DesktopHost::DesktopHost(const WidgetRegistry& registry, std::shared_ptr<MediaSessionService> mediaSession)
    : registry_(registry), mediaSession_(std::move(mediaSession)), scene_(registry),
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
    instance_ = instance;
    if (!RegisterWindowClass(instance_)) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        760,
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

    dpi_ = std::max(96u, GetDpiForWindow(hwnd_));
    if (FAILED(renderer_.Initialize(hwnd_))) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    if (!tray_.Initialize(hwnd_)) {
        tray_.Shutdown();
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }
    hotkeyRegistered_ = RegisterHotKey(
        hwnd_, kHotkeyToggleEdit, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'W') != FALSE;

    monitorTopology_.Refresh();
    activeMonitorId_ = HostMonitorId();
    if (!desktopBackend_.AttachConfigured(hwnd_, ActiveDesktopTarget())) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }
    UpdateMetrics();
    const SceneLoadStatus loadStatus = LoadScene();
    if (loadStatus != SceneLoadStatus::Loaded) {
        CreateWidget("clock", loadStatus == SceneLoadStatus::Missing);
    }
    if (monitorTopology_.MigrateMissingWidgets(
            scene_, grid_.Columns(), grid_.Rows()) > 0) SaveScene();
    RebuildSecondarySurfaces();
    ScheduleNextWidgetUpdate();
    ShowWindow(hwnd_, showCommand);
    UpdateWindow(hwnd_);
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

PointF DesktopHost::ClientPointFromLParam(LPARAM lParam) const noexcept {
    return ClientPointFromLParam(lParam, dpi_);
}

PointF DesktopHost::ClientPointFromLParam(LPARAM lParam, UINT dpi) noexcept {
    const float pixelsToDips = 96.0f / static_cast<float>(std::max(1u, dpi));
    return PointF{
        static_cast<float>(GET_X_LPARAM(lParam)) * pixelsToDips,
        static_cast<float>(GET_Y_LPARAM(lParam)) * pixelsToDips,
    };
}

RectF DesktopHost::ClientBounds() const noexcept {
    RECT rect{};
    if (!hwnd_ || !GetClientRect(hwnd_, &rect)) return {};
    const float pixelsToDips = 96.0f / static_cast<float>(std::max(1u, dpi_));
    return RectF{0.0f, 0.0f,
        static_cast<float>(rect.right - rect.left) * pixelsToDips,
        static_cast<float>(rect.bottom - rect.top) * pixelsToDips};
}

std::optional<DesktopHost::WidgetActionHit> DesktopHost::HitTestWidgetAction(PointF point) const {
    return HitTestWidgetAction(point, metrics_, HostMonitorId());
}

std::optional<DesktopHost::WidgetActionHit> DesktopHost::HitTestWidgetAction(
    PointF point, const GridMetrics& metrics, std::wstring_view monitorId) const {
    const auto instanceId = scene_.HitTest(point, grid_, metrics, monitorId);
    if (!instanceId) return std::nullopt;
    const WidgetInstance* widget = scene_.Find(*instanceId);
    if (!widget || !widget->content) return std::nullopt;
    const RectF outer = OuterLayout::RectFor(*widget, grid_, metrics);
    const auto action = widget->content->HitTestAction(WidgetHitTestContext{
        .point = point,
        .bounds = WidgetVisualStyle::ContentBounds(outer),
        .contentScale = widget->contentScale,
    });
    if (!action) return std::nullopt;
    return WidgetActionHit{*instanceId, *action};
}

void DesktopHost::UpdateMetrics() {
    if (!hwnd_) return;
    RECT rc{};
    if (!GetClientRect(hwnd_, &rc)) return;
    const float pixelsToDips = 96.0f / static_cast<float>(std::max(1u, dpi_));
    metrics_ = grid_.Calculate(SizeF{
        static_cast<float>(rc.right - rc.left) * pixelsToDips,
        static_cast<float>(rc.bottom - rc.top) * pixelsToDips,
    });
    if (activeMonitorId_.empty()) activeMonitorId_ = HostMonitorId();
    if (activeMonitorId_ == HostMonitorId()) {
        activeMetrics_ = metrics_;
        activeBounds_ = ClientBounds();
        studio_.UpdateLayoutContext(metrics_, activeBounds_, activeMonitorId_);
    }
}

void DesktopHost::ToggleEditMode() {
    SetEditMode(!editMode_);
}

void DesktopHost::SetEditMode(bool enabled) {
    editMode_ = enabled;
    if (!editMode_) {
        EndDrag();
    }
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
    const HWND owner = desktopBackend_.IsExperimental() ? nullptr : hwnd_;
    library_.Open(owner, instance_, registry_, [this](std::string typeId) { CreateWidget(typeId); });
}

void DesktopHost::OpenWidgetStudio() {
    const std::filesystem::path assetDirectory = sceneStore_.ConfigPath().parent_path() / L"assets";
    const GridMetrics studioMetrics = activeBounds_.width > 0.0f ? activeMetrics_ : metrics_;
    const RectF studioBounds = activeBounds_.width > 0.0f ? activeBounds_ : ClientBounds();
    const HWND owner = desktopBackend_.IsExperimental() ? nullptr : hwnd_;
    if (!studio_.Open(owner, instance_, scene_, grid_, studioMetrics, studioBounds, assetDirectory,
            ActiveMonitorId(), [this] {
        SaveScene();
        ScheduleNextWidgetUpdate();
        InvalidateDesktop();
    }, [this] { InvalidateDesktop(); }, [this] { OpenWidgetLibrary(); })) {
        MessageBoxW(hwnd_, L"Widget Studio could not open its settings window.",
            L"Widget Studio", MB_OK | MB_ICONERROR);
    }
}

void DesktopHost::CreateWidget(std::string_view typeId, bool persist) {
    if (!scene_.CreateWidget(typeId, ActiveMonitorId())) return;
    SetEditMode(true);
    if (persist) SaveScene();
    ScheduleNextWidgetUpdate();
    InvalidateDesktop();
    studio_.Refresh();
}

std::wstring DesktopHost::ActiveMonitorId() const {
    return activeMonitorId_.empty() ? HostMonitorId() : activeMonitorId_;
}

std::wstring DesktopHost::HostMonitorId() const {
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    const HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    if (monitor && GetMonitorInfoW(monitor, reinterpret_cast<MONITORINFO*>(&info))) return info.szDevice;
    return L"primary";
}

DesktopTargetBounds DesktopHost::ActiveDesktopTarget() const {
    const MonitorDescriptor* monitor = monitorTopology_.Find(HostMonitorId());
    if (!monitor) monitor = monitorTopology_.Primary();
    if (!monitor) return {};
    return DesktopTargetBounds{monitor->pixelX, monitor->pixelY,
        monitor->pixelWidth, monitor->pixelHeight, true};
}

void DesktopHost::ActivateMonitor(
    std::wstring_view monitorId, const GridMetrics& metrics, RectF bounds) {
    activeMonitorId_ = monitorId;
    activeMetrics_ = metrics;
    activeBounds_ = bounds;
    studio_.UpdateLayoutContext(metrics, bounds, activeMonitorId_);
}

void DesktopHost::InvalidateDesktop(bool reloadWallpaper) {
    if (reloadWallpaper) static_cast<void>(renderer_.ReloadWallpaper());
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
    for (const auto& surface : secondarySurfaces_) surface->Invalidate(reloadWallpaper);
}

void DesktopHost::RebuildSecondarySurfaces() {
    secondarySurfaces_.clear();
    if (!desktopBackend_.IsExperimental()) return;
    const std::wstring hostMonitor = HostMonitorId();
    for (const MonitorDescriptor& monitor : monitorTopology_.Monitors()) {
        if (monitor.id == hostMonitor) continue;
        auto surface = std::make_unique<DesktopSurface>();
        if (surface->Create(*this, instance_, monitor)) secondarySurfaces_.push_back(std::move(surface));
    }
    const auto active = std::find_if(secondarySurfaces_.begin(), secondarySurfaces_.end(),
        [this](const auto& surface) { return surface->MonitorId() == activeMonitorId_; });
    if (active != secondarySurfaces_.end()) {
        ActivateMonitor((*active)->MonitorId(), (*active)->Metrics(), (*active)->Bounds());
    } else {
        ActivateMonitor(HostMonitorId(), metrics_, ClientBounds());
    }
}

void DesktopHost::RefreshMonitorConfiguration() {
    secondarySurfaces_.clear();
    if (!monitorTopology_.Refresh()) return;
    if (monitorTopology_.MigrateMissingWidgets(
            scene_, grid_.Columns(), grid_.Rows()) > 0) SaveScene();
    desktopBackend_.Detach(hwnd_);
    static_cast<void>(desktopBackend_.AttachConfigured(hwnd_, ActiveDesktopTarget()));
    UpdateMetrics();
    ActivateMonitor(HostMonitorId(), metrics_, ClientBounds());
    RebuildSecondarySurfaces();
    InvalidateDesktop();
    studio_.Refresh();
}

void DesktopHost::DeleteSelectedWidgets() {
    EndDrag();
    if (scene_.RemoveSelectedWidgets() > 0) {
        SaveScene();
        ScheduleNextWidgetUpdate();
        InvalidateDesktop();
        studio_.Refresh();
    }
}

void DesktopHost::DuplicatePrimaryWidget() {
    const auto primary = scene_.PrimarySelection();
    if (primary && scene_.DuplicateWidget(*primary)) {
        SaveScene();
        ScheduleNextWidgetUpdate();
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
    std::optional<std::chrono::system_clock::time_point> nextUpdate;
    for (const auto& widget : scene_.Widgets()) {
        if (!widget.content) continue;
        const auto candidate = widget.content->NextUpdateTime();
        if (candidate && (!nextUpdate || *candidate < *nextUpdate)) nextUpdate = candidate;
    }
    if (!nextUpdate) return;
    const auto now = std::chrono::system_clock::now();
    const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(*nextUpdate - now).count();
    const auto delay = static_cast<UINT>(std::clamp<long long>(
        remaining, USER_TIMER_MINIMUM, static_cast<long long>(std::numeric_limits<UINT>::max())));
    SetTimer(hwnd_, kWidgetUpdateTimer, delay, nullptr);
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

void DesktopHost::Paint() {
    PAINTSTRUCT ps{};
    BeginPaint(hwnd_, &ps);
    const HRESULT renderResult = renderer_.Render(
        scene_, grid_, metrics_, editMode_, 1.0f, {}, HostMonitorId());
    EndPaint(hwnd_, &ps);
    if (renderResult == D2DERR_RECREATE_TARGET) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

LRESULT DesktopHost::HandleSurfaceNcHitTest(
    DesktopSurface& surface, WPARAM wParam, LPARAM lParam) {
    const LRESULT defaultResult = DefWindowProcW(surface.Window(), WM_NCHITTEST, wParam, lParam);
    if (defaultResult != HTCLIENT || editMode_) return defaultResult;
    POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (!ScreenToClient(surface.Window(), &point)) return HTTRANSPARENT;
    const float pixelsToDips = 96.0f / static_cast<float>(std::max(1u, surface.Dpi()));
    const PointF clientPoint{static_cast<float>(point.x) * pixelsToDips,
        static_cast<float>(point.y) * pixelsToDips};
    return HitTestWidgetAction(clientPoint, surface.Metrics(), surface.MonitorId())
        ? HTCLIENT : HTTRANSPARENT;
}

void DesktopHost::HandleSurfaceLeftDown(
    DesktopSurface& surface, WPARAM, LPARAM lParam) {
    ActivateMonitor(surface.MonitorId(), surface.Metrics(), surface.Bounds());
    const PointF point = ClientPointFromLParam(lParam, surface.Dpi());
    if (const auto action = HitTestWidgetAction(point, surface.Metrics(), surface.MonitorId())) {
        WidgetInstance* widget = scene_.Find(action->instanceId);
        if (widget && widget->content) static_cast<void>(widget->content->InvokeAction(action->actionId));
        ScheduleNextWidgetUpdate();
        surface.Invalidate();
        return;
    }
    if (!editMode_) return;
    const auto hit = scene_.HitTest(point, grid_, surface.Metrics(), surface.MonitorId());
    if (!hit) {
        scene_.ClearSelection();
        InvalidateDesktop();
        studio_.Refresh();
        return;
    }
    scene_.Select(*hit, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
    const WidgetInstance* selected = scene_.Find(*hit);
    if (selected && selected->selected) {
        BeginDrag(*hit, point, surface.Window(), surface.Metrics(), surface.Bounds());
    }
    InvalidateDesktop();
    studio_.Refresh();
}

void DesktopHost::HandleSurfaceMouseMove(
    DesktopSurface& surface, WPARAM wParam, LPARAM lParam) {
    if (editMode_ && drag_ && drag_->captureWindow == surface.Window() && (wParam & MK_LBUTTON)) {
        UpdateDrag(ClientPointFromLParam(lParam, surface.Dpi()));
    } else if (drag_ && drag_->captureWindow == surface.Window() && !(wParam & MK_LBUTTON)) {
        EndDrag();
    }
}

bool DesktopHost::HandleSurfaceKeyDown(WPARAM key) {
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
        desktopBackend_.Reattach(hwnd_);
        RebuildSecondarySurfaces();
        UpdateMetrics();
        InvalidateDesktop();
        return 0;
    }
    switch (message) {
    case WM_NCHITTEST: {
        const LRESULT defaultResult = DefWindowProcW(hwnd_, message, wParam, lParam);
        if (defaultResult != HTCLIENT || editMode_) return defaultResult;
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (!ScreenToClient(hwnd_, &point)) return HTTRANSPARENT;
        const float pixelsToDips = 96.0f / static_cast<float>(std::max(1u, dpi_));
        const PointF clientPoint{static_cast<float>(point.x) * pixelsToDips,
            static_cast<float>(point.y) * pixelsToDips};
        return HitTestWidgetAction(clientPoint) ? HTCLIENT : HTTRANSPARENT;
    }

    case WM_SIZE:
        UpdateMetrics();
        renderer_.Resize(LOWORD(lParam), HIWORD(lParam));
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;

    case WM_MOVE:
        if (!desktopBackend_.IsExperimental()) {
            const std::wstring hostMonitor = HostMonitorId();
            ActivateMonitor(hostMonitor, metrics_, ClientBounds());
        }
        return 0;

    case WM_DPICHANGED: {
        dpi_ = std::max(96u, static_cast<UINT>(HIWORD(wParam)));
        renderer_.SetDpi(static_cast<float>(dpi_));
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(
            hwnd_,
            nullptr,
            suggested->left,
            suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        UpdateMetrics();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    case WM_SETTINGCHANGE:
        if (wParam == SPI_SETWORKAREA) RefreshMonitorConfiguration();
        InvalidateDesktop(true);
        studio_.InvalidatePreview(true);
        ScheduleNextWidgetUpdate();
        return 0;

    case WM_DISPLAYCHANGE:
        RefreshMonitorConfiguration();
        return 0;

    case WM_TIMECHANGE:
        ScheduleNextWidgetUpdate();
        InvalidateDesktop();
        studio_.InvalidatePreview();
        return 0;

    case WM_TIMER:
        if (wParam == kWidgetUpdateTimer) {
            ScheduleNextWidgetUpdate();
            InvalidateDesktop();
            studio_.InvalidatePreview();
            return 0;
        }
        break;

    case kMediaSessionChangedMessage:
        ScheduleNextWidgetUpdate();
        InvalidateDesktop();
        studio_.InvalidatePreview();
        return 0;

    case WM_PAINT:
        Paint();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_LBUTTONDOWN: {
        ActivateMonitor(HostMonitorId(), metrics_, ClientBounds());
        const PointF point = ClientPointFromLParam(lParam);
        if (const auto action = HitTestWidgetAction(point)) {
            WidgetInstance* widget = scene_.Find(action->instanceId);
            if (widget && widget->content) {
                static_cast<void>(widget->content->InvokeAction(action->actionId));
            }
            ScheduleNextWidgetUpdate();
            InvalidateDesktop();
            return 0;
        }
        if (!editMode_) return 0;
        const auto hit = scene_.HitTest(point, grid_, metrics_, HostMonitorId());
        if (!hit) {
            scene_.ClearSelection();
            InvalidateDesktop();
            studio_.Refresh();
            return 0;
        }

        const bool additive = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        scene_.Select(*hit, additive);
        const WidgetInstance* selected = scene_.Find(*hit);
        if (selected && selected->selected) {
            BeginDrag(*hit, point, hwnd_, metrics_, ClientBounds());
        }
        InvalidateDesktop();
        studio_.Refresh();
        return 0;
    }

    case WM_MOUSEMOVE:
        if (editMode_ && drag_ && drag_->captureWindow == hwnd_ && (wParam & MK_LBUTTON)) {
            UpdateDrag(ClientPointFromLParam(lParam));
        } else if (drag_ && drag_->captureWindow == hwnd_ && !(wParam & MK_LBUTTON)) {
            EndDrag();
        }
        return 0;

    case WM_LBUTTONUP:
        EndDrag();
        return 0;

    case WM_CAPTURECHANGED:
        EndDrag();
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && editMode_) {
            SetEditMode(false);
            return 0;
        }
        if (editMode_ && wParam == VK_DELETE) {
            DeleteSelectedWidgets();
            return 0;
        }
        if (editMode_ && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            if (wParam == 'D') {
                DuplicatePrimaryWidget();
                return 0;
            }
            if (wParam == 'L') {
                TogglePrimaryWidgetLock();
                return 0;
            }
        }
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
            tray_.ShowContextMenu(editMode_);
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
        secondarySurfaces_.clear();
        desktopBackend_.Detach(hwnd_);
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
