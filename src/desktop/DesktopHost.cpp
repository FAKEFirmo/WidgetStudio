#include "desktop/DesktopHost.h"

#include <algorithm>
#include <windowsx.h>

namespace ws {
namespace {

constexpr wchar_t kWindowClassName[] = L"WidgetStudioHostWindow";
constexpr wchar_t kWindowTitle[] = L"Widget Studio - Development Host";
constexpr int kHotkeyToggleEdit = 1;

} // namespace

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

void DesktopHost::BeginDrag(std::uint64_t widgetId, PointF pointer) {
    WidgetInstance* widget = scene_.Find(widgetId);
    if (!widget || widget->locked) return;

    const RectF rect = grid_.RectFor(widget->grid, metrics_);
    drag_ = DragState{
        .widgetId = widgetId,
        .offset = PointF{pointer.x - rect.x, pointer.y - rect.y},
    };
    SetCapture(hwnd_);
}

void DesktopHost::UpdateDrag(PointF pointer) {
    if (!drag_) return;

    WidgetInstance* widget = scene_.Find(drag_->widgetId);
    if (!widget || widget->locked) return;

    widget->grid = grid_.MoveToPoint(widget->grid, pointer, drag_->offset, metrics_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void DesktopHost::EndDrag() {
    if (drag_) {
        drag_.reset();
        if (GetCapture() == hwnd_) {
            ReleaseCapture();
        }
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
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;

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
        drag_.reset();
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && editMode_) {
            SetEditMode(false);
            return 0;
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
