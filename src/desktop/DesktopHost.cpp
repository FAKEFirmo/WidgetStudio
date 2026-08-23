#include "desktop/DesktopHost.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <windowsx.h>

namespace ws {
namespace {

constexpr wchar_t kWindowClassName[] = L"WidgetStudioHostWindow";
constexpr wchar_t kWindowTitle[] = L"Widget Studio - Development Host";
constexpr int kHotkeyToggleEdit = 1;
constexpr UINT_PTR kWidgetUpdateTimer = 2;

} // namespace

DesktopHost::DesktopHost(const WidgetRegistry& registry)
    : registry_(registry), scene_(registry), sceneStore_(SceneStore::DefaultConfigPath()) {
    scene_.SetGridDimensions(grid_.Columns(), grid_.Rows());
}

DesktopHost::~DesktopHost() {
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

    dpi_ = std::max(96u, GetDpiForWindow(hwnd_));
    if (FAILED(renderer_.Initialize(hwnd_))) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    if (!tray_.Initialize(hwnd_) ||
        !RegisterHotKey(hwnd_, kHotkeyToggleEdit, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'W')) {
        tray_.Shutdown();
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    UpdateMetrics();
    const SceneLoadStatus loadStatus = LoadScene();
    if (loadStatus != SceneLoadStatus::Loaded) {
        CreateWidget("clock", loadStatus == SceneLoadStatus::Missing);
    }
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
    const float pixelsToDips = 96.0f / static_cast<float>(std::max(1u, dpi_));
    return PointF{
        static_cast<float>(GET_X_LPARAM(lParam)) * pixelsToDips,
        static_cast<float>(GET_Y_LPARAM(lParam)) * pixelsToDips,
    };
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
}

void DesktopHost::ToggleEditMode() {
    SetEditMode(!editMode_);
}

void DesktopHost::SetEditMode(bool enabled) {
    editMode_ = enabled;
    if (!editMode_) {
        EndDrag();
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void DesktopHost::BeginDrag(std::string_view widgetId, PointF pointer) {
    WidgetInstance* widget = scene_.Find(widgetId);
    if (!widget || widget->locked) return;

    const RectF rect = grid_.RectFor(widget->grid, metrics_);
    drag_ = DragState{
        .widgetId = std::string(widgetId),
        .offset = PointF{pointer.x - rect.x, pointer.y - rect.y},
        .moved = false,
    };
    SetCapture(hwnd_);
}

void DesktopHost::OpenWidgetLibrary() {
    library_.Open(hwnd_, instance_, registry_, [this](std::string typeId) { CreateWidget(typeId); });
}

void DesktopHost::CreateWidget(std::string_view typeId, bool persist) {
    if (!scene_.CreateWidget(typeId, ActiveMonitorId())) return;
    SetEditMode(true);
    if (persist) SaveScene();
    ScheduleNextWidgetUpdate();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

std::wstring DesktopHost::ActiveMonitorId() const {
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    const HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    if (monitor && GetMonitorInfoW(monitor, reinterpret_cast<MONITORINFO*>(&info))) return info.szDevice;
    return L"primary";
}

void DesktopHost::DeleteSelectedWidgets() {
    EndDrag();
    if (scene_.RemoveSelectedWidgets() > 0) {
        SaveScene();
        ScheduleNextWidgetUpdate();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void DesktopHost::DuplicatePrimaryWidget() {
    const auto primary = scene_.PrimarySelection();
    if (primary && scene_.DuplicateWidget(*primary)) {
        SaveScene();
        ScheduleNextWidgetUpdate();
        InvalidateRect(hwnd_, nullptr, FALSE);
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
    InvalidateRect(hwnd_, nullptr, FALSE);
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

    const GridPlacement moved = grid_.MoveToPoint(widget->grid, pointer, drag_->offset, metrics_);
    if (moved.column != widget->grid.column || moved.row != widget->grid.row) {
        widget->grid = moved;
        drag_->moved = true;
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void DesktopHost::EndDrag() {
    if (drag_) {
        const bool moved = drag_->moved;
        drag_.reset();
        if (GetCapture() == hwnd_) {
            ReleaseCapture();
        }
        if (moved) SaveScene();
    }
}

void DesktopHost::Paint() {
    PAINTSTRUCT ps{};
    BeginPaint(hwnd_, &ps);
    const HRESULT renderResult = renderer_.Render(scene_, grid_, metrics_, editMode_);
    EndPaint(hwnd_, &ps);
    if (renderResult == D2DERR_RECREATE_TARGET) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

LRESULT DesktopHost::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_SIZE:
        UpdateMetrics();
        renderer_.Resize(LOWORD(lParam), HIWORD(lParam));
        InvalidateRect(hwnd_, nullptr, FALSE);
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
        renderer_.ReloadWallpaper();
        ScheduleNextWidgetUpdate();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;

    case WM_TIMECHANGE:
        ScheduleNextWidgetUpdate();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;

    case WM_TIMER:
        if (wParam == kWidgetUpdateTimer) {
            ScheduleNextWidgetUpdate();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        break;

    case WM_PAINT:
        Paint();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_LBUTTONDOWN: {
        if (!editMode_) return 0;
        const PointF point = ClientPointFromLParam(lParam);
        const auto hit = scene_.HitTest(point, grid_, metrics_);
        if (!hit) {
            scene_.ClearSelection();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        const bool additive = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        scene_.Select(*hit, additive);
        const WidgetInstance* selected = scene_.Find(*hit);
        if (selected && selected->selected) {
            BeginDrag(*hit, point);
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE:
        if (editMode_ && drag_ && (wParam & MK_LBUTTON)) {
            UpdateDrag(ClientPointFromLParam(lParam));
        } else if (drag_ && !(wParam & MK_LBUTTON)) {
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
        KillTimer(hwnd_, kWidgetUpdateTimer);
        UnregisterHotKey(hwnd_, kHotkeyToggleEdit);
        tray_.Shutdown();
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

} // namespace ws
