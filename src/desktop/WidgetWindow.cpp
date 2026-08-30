#include "desktop/WidgetWindow.h"

#include "desktop/DesktopHost.h"
#include "desktop/WidgetWindowPlacement.h"

#include <algorithm>
#include <utility>

namespace ws {
namespace {

constexpr wchar_t kWidgetWindowClassName[] = L"WidgetStudioDesktopWidgetWindow";

bool RegisterWidgetWindowClass(HINSTANCE instance) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WidgetWindow::WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWidgetWindowClassName;
    return RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

} // namespace

WidgetWindow::~WidgetWindow() { Close(); }

bool WidgetWindow::Create(DesktopHost& host, HINSTANCE instance, std::string instanceId,
    const MonitorDescriptor& monitor) {
    host_ = &host;
    instance_ = instance;
    instanceId_ = std::move(instanceId);
    if (!RegisterWidgetWindowClass(instance_)) return false;

    hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kWidgetWindowClassName, L"WidgetStudio widget", WS_POPUP,
        0, 0, 1, 1, nullptr, nullptr, instance_, this);
    if (!hwnd_) return false;
    if (FAILED(renderer_.Initialize(hwnd_)) || !UpdatePlacement(monitor)) {
        Close();
        return false;
    }
    SetEditMode(host.EditMode());
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    return true;
}

void WidgetWindow::Close() noexcept {
    if (hwnd_ && IsWindow(hwnd_)) {
        backend_.Detach(hwnd_);
        DestroyWindow(hwnd_);
    }
    hwnd_ = nullptr;
}

bool WidgetWindow::UpdatePlacement(const MonitorDescriptor& monitor) {
    if (!host_ || !hwnd_) return false;
    const WidgetInstance* widget = host_->Scene().Find(instanceId_);
    if (!widget) return false;

    monitorId_ = monitor.id;
    dpi_ = std::max(96u, monitor.dpi);
    monitorSize_ = {monitor.workAreaDips.width, monitor.workAreaDips.height};
    metrics_ = host_->Grid().Calculate(monitorSize_);
    const WidgetWindowPlacement placement = WidgetWindowPlacementCalculator::Calculate(
        *widget, host_->Grid(), metrics_, monitor);
    widgetBounds_ = placement.widgetDips;
    windowBounds_ = placement.windowDips;
    widgetBoundsInWindow_ = placement.widgetInWindowDips;
    const DesktopTargetBounds target{
        placement.screenX,
        placement.screenY,
        placement.pixelWidth,
        placement.pixelHeight,
        true,
    };
    renderer_.SetDpi(static_cast<float>(dpi_));
    const bool positioned = backend_.UpdateTarget(hwnd_, target);
    renderer_.Resize(static_cast<UINT>(target.width), static_cast<UINT>(target.height));
    return positioned;
}

void WidgetWindow::SetEditMode(bool enabled) {
    if (!hwnd_) return;
    LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    style = enabled ? (style & ~WS_EX_NOACTIVATE) : (style | WS_EX_NOACTIVATE);
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, style);
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void WidgetWindow::Invalidate(bool reloadWallpaper) {
    if (!hwnd_) return;
    if (reloadWallpaper) static_cast<void>(renderer_.ReloadWallpaper());
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void WidgetWindow::Reattach() {
    if (hwnd_) backend_.Reattach(hwnd_);
}

void WidgetWindow::Paint() {
    PAINTSTRUCT paint{};
    BeginPaint(hwnd_, &paint);
    const WidgetInstance* widget = host_ ? host_->Scene().Find(instanceId_) : nullptr;
    const HRESULT result = widget
        ? renderer_.RenderWidget(*widget, host_->EditMode(),
            windowBounds_, monitorSize_, widgetBoundsInWindow_)
        : S_FALSE;
    EndPaint(hwnd_, &paint);
    if (result == D2DERR_RECREATE_TARGET) InvalidateRect(hwnd_, nullptr, FALSE);
}

LRESULT CALLBACK WidgetWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    WidgetWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        self = static_cast<WidgetWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<WidgetWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProcW(hwnd, message, wParam, lParam);
    const LRESULT result = self->HandleMessage(message, wParam, lParam);
    if (message == WM_NCDESTROY) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        self->hwnd_ = nullptr;
    }
    return result;
}

LRESULT WidgetWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_NCHITTEST:
        return host_->HandleWidgetNcHitTest(*this, wParam, lParam);
    case WM_PAINT:
        Paint();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        host_->HandleWidgetLeftDown(*this, wParam, lParam);
        return 0;
    case WM_MOUSEMOVE:
        host_->HandleWidgetMouseMove(*this, wParam, lParam);
        return 0;
    case WM_LBUTTONUP:
    case WM_CAPTURECHANGED:
        host_->EndDrag();
        return 0;
    case WM_KEYDOWN:
        if (host_->HandleWidgetKeyDown(wParam)) return 0;
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

} // namespace ws
