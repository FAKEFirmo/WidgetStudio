#include "desktop/DesktopSurface.h"

#include "desktop/DesktopHost.h"

#include <algorithm>
#include <windowsx.h>

namespace ws {
namespace {

constexpr wchar_t kSurfaceClassName[] = L"WidgetStudioSecondarySurfaceWindow";

bool RegisterSurfaceClass(HINSTANCE instance) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = DesktopSurface::WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kSurfaceClassName;
    return RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

} // namespace

DesktopSurface::~DesktopSurface() { Close(); }

bool DesktopSurface::Create(
    DesktopHost& host, HINSTANCE instance, const MonitorDescriptor& monitor) {
    host_ = &host;
    instance_ = instance;
    monitorId_ = monitor.id;
    grid_ = &host.Grid();
    if (!RegisterSurfaceClass(instance_)) return false;
    hwnd_ = CreateWindowExW(0, kSurfaceClassName, L"WidgetStudio monitor surface",
        WS_OVERLAPPEDWINDOW, monitor.pixelX, monitor.pixelY,
        std::max(1, monitor.pixelWidth), std::max(1, monitor.pixelHeight),
        nullptr, nullptr, instance_, this);
    if (!hwnd_) return false;
    dpi_ = std::max(96u, GetDpiForWindow(hwnd_));
    if (FAILED(renderer_.Initialize(hwnd_))) { Close(); return false; }
    const DesktopTargetBounds target{monitor.pixelX, monitor.pixelY,
        monitor.pixelWidth, monitor.pixelHeight, true};
    if (!backend_.AttachConfigured(hwnd_, target) || !backend_.IsExperimental()) {
        Close();
        return false;
    }
    UpdateMetrics();
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    return true;
}

void DesktopSurface::Close() noexcept {
    if (hwnd_ && IsWindow(hwnd_)) {
        backend_.Detach(hwnd_);
        DestroyWindow(hwnd_);
    }
    hwnd_ = nullptr;
}

void DesktopSurface::Invalidate(bool reloadWallpaper) {
    if (!hwnd_) return;
    if (reloadWallpaper) static_cast<void>(renderer_.ReloadWallpaper());
    InvalidateRect(hwnd_, nullptr, FALSE);
}

RectF DesktopSurface::Bounds() const noexcept {
    RECT client{};
    if (!hwnd_ || !GetClientRect(hwnd_, &client)) return {};
    const float pixelsToDips = 96.0f / static_cast<float>(std::max(1u, dpi_));
    return {0.0f, 0.0f, static_cast<float>(client.right) * pixelsToDips,
        static_cast<float>(client.bottom) * pixelsToDips};
}

void DesktopSurface::UpdateMetrics() {
    if (!grid_) return;
    const RectF bounds = Bounds();
    metrics_ = grid_->Calculate({bounds.width, bounds.height});
}

void DesktopSurface::Paint() {
    PAINTSTRUCT paint{};
    BeginPaint(hwnd_, &paint);
    const HRESULT result = renderer_.Render(host_->Scene(), *grid_, metrics_, host_->EditMode(),
        1.0f, {}, monitorId_);
    EndPaint(hwnd_, &paint);
    if (result == D2DERR_RECREATE_TARGET) InvalidateRect(hwnd_, nullptr, FALSE);
}

LRESULT CALLBACK DesktopSurface::WindowProc(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    DesktopSurface* self = nullptr;
    if (message == WM_NCCREATE) {
        self = static_cast<DesktopSurface*>(
            reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<DesktopSurface*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProcW(hwnd, message, wParam, lParam);
    const LRESULT result = self->HandleMessage(message, wParam, lParam);
    if (message == WM_NCDESTROY) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        self->hwnd_ = nullptr;
    }
    return result;
}

LRESULT DesktopSurface::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_NCHITTEST:
        return host_->HandleSurfaceNcHitTest(*this, wParam, lParam);
    case WM_SIZE:
        UpdateMetrics();
        renderer_.Resize(LOWORD(lParam), HIWORD(lParam));
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_DPICHANGED: {
        dpi_ = std::max(96u, static_cast<UINT>(HIWORD(wParam)));
        renderer_.SetDpi(static_cast<float>(dpi_));
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        UpdateMetrics();
        return 0;
    }
    case WM_PAINT: Paint(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN:
        host_->HandleSurfaceLeftDown(*this, wParam, lParam);
        return 0;
    case WM_MOUSEMOVE:
        host_->HandleSurfaceMouseMove(*this, wParam, lParam);
        return 0;
    case WM_LBUTTONUP:
    case WM_CAPTURECHANGED:
        host_->EndDrag();
        return 0;
    case WM_KEYDOWN:
        if (host_->HandleSurfaceKeyDown(wParam)) return 0;
        break;
    default: break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

} // namespace ws
