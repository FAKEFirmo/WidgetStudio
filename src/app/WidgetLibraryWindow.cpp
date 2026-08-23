#include "app/WidgetLibraryWindow.h"

#include "widgets/WidgetRegistry.h"

#include <algorithm>
#include <utility>

namespace ws {
namespace {

constexpr wchar_t kLibraryClassName[] = L"WidgetStudioLibraryWindow";
constexpr int kListId = 100;
constexpr int kAddId = 101;

} // namespace

WidgetLibraryWindow::~WidgetLibraryWindow() { Close(); }

bool WidgetLibraryWindow::Open(HWND owner, HINSTANCE instance, const WidgetRegistry& registry,
    std::function<void(std::string)> createWidget) {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_SHOWNORMAL);
        SetForegroundWindow(hwnd_);
        return true;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));
    wc.lpszClassName = kLibraryClassName;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    registry_ = &registry;
    createWidget_ = std::move(createWidget);
    hwnd_ = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_APPWINDOW, kLibraryClassName, L"Add Widget",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        430, 330, owner, nullptr, instance, this);
    if (!hwnd_) return false;
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    UpdateWindow(hwnd_);
    return true;
}

void WidgetLibraryWindow::Close() noexcept {
    if (hwnd_ && IsWindow(hwnd_)) DestroyWindow(hwnd_);
    hwnd_ = nullptr;
}

LRESULT CALLBACK WidgetLibraryWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    WidgetLibraryWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<WidgetLibraryWindow*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<WidgetLibraryWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return self ? self->HandleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT WidgetLibraryWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_GETMINMAXINFO: {
        const float scale = static_cast<float>(std::max(96u, GetDpiForWindow(hwnd_))) / 96.0f;
        auto* bounds = reinterpret_cast<MINMAXINFO*>(lParam);
        bounds->ptMinTrackSize.x = static_cast<LONG>(360.0f * scale);
        bounds->ptMinTrackSize.y = static_cast<LONG>(280.0f * scale);
        return 0;
    }
    case WM_CREATE: {
        const HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd_, GWLP_HINSTANCE));
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        list_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY,
            16, 16, 382, 150, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kListId)), instance, nullptr);
        description_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
            16, 178, 382, 52, hwnd_, nullptr, instance, nullptr);
        add_ = CreateWindowExW(0, L"BUTTON", L"Add Widget",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            286, 242, 112, 32, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kAddId)), instance, nullptr);
        SendMessageW(list_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(description_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(add_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        for (const auto& descriptor : registry_->Descriptors()) {
            SendMessageW(list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(descriptor.displayName.c_str()));
        }
        if (!registry_->Descriptors().empty()) {
            SendMessageW(list_, LB_SETCURSEL, 0, 0);
            UpdateDescription();
        }
        return 0;
    }
    case WM_SIZE:
        LayoutControls(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == kAddId ||
            (LOWORD(wParam) == kListId && HIWORD(wParam) == LBN_DBLCLK)) {
            CreateSelectedWidget();
            return 0;
        }
        if (LOWORD(wParam) == kListId && HIWORD(wParam) == LBN_SELCHANGE) {
            UpdateDescription();
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd_);
        return 0;
    case WM_NCDESTROY:
        DefWindowProcW(hwnd_, message, wParam, lParam);
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        hwnd_ = nullptr;
        list_ = nullptr;
        description_ = nullptr;
        add_ = nullptr;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

void WidgetLibraryWindow::LayoutControls(int width, int height) {
    if (!list_ || !description_ || !add_) return;
    const float scale = static_cast<float>(std::max(96u, GetDpiForWindow(hwnd_))) / 96.0f;
    const int margin = static_cast<int>(16.0f * scale);
    const int gap = static_cast<int>(12.0f * scale);
    const int buttonWidth = static_cast<int>(112.0f * scale);
    const int buttonHeight = static_cast<int>(32.0f * scale);
    const int descriptionHeight = static_cast<int>(52.0f * scale);
    const int contentWidth = std::max(1, width - margin * 2);
    const int listHeight = std::max(static_cast<int>(120.0f * scale),
        height - margin * 2 - gap * 2 - descriptionHeight - buttonHeight);
    MoveWindow(list_, margin, margin, contentWidth, listHeight, TRUE);
    MoveWindow(description_, margin, margin + listHeight + gap,
        contentWidth, descriptionHeight, TRUE);
    MoveWindow(add_, std::max(margin, width - margin - buttonWidth),
        std::max(margin, height - margin - buttonHeight), buttonWidth, buttonHeight, TRUE);
}

void WidgetLibraryWindow::CreateSelectedWidget() {
    const LRESULT selected = SendMessageW(list_, LB_GETCURSEL, 0, 0);
    const auto& descriptors = registry_->Descriptors();
    if (selected == LB_ERR || static_cast<std::size_t>(selected) >= descriptors.size()) return;
    createWidget_(descriptors[static_cast<std::size_t>(selected)].typeId);
}

void WidgetLibraryWindow::UpdateDescription() {
    const LRESULT selected = SendMessageW(list_, LB_GETCURSEL, 0, 0);
    const auto& descriptors = registry_->Descriptors();
    if (selected == LB_ERR || static_cast<std::size_t>(selected) >= descriptors.size()) return;
    SetWindowTextW(description_, descriptors[static_cast<std::size_t>(selected)].description.c_str());
}

} // namespace ws
