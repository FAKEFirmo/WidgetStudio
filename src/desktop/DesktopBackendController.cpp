#include "desktop/DesktopBackendController.h"

#include <algorithm>
#include <array>
#include <cwchar>
#include <utility>

namespace ws {
namespace {

class WindowedDesktopBackend final : public IDesktopBackend {
public:
    std::wstring_view Name() const noexcept override { return L"Windowed"; }
    bool Attach(HWND host, DesktopTargetBounds target) override {
        if (!host || !IsWindow(host)) return false;
        if (!target.valid) return true;
        return SetWindowPos(host, HWND_BOTTOM, target.x, target.y,
            std::max(1, target.width), std::max(1, target.height),
            SWP_NOACTIVATE | SWP_SHOWWINDOW) != FALSE;
    }
    void Detach(HWND) noexcept override {}
};

class WorkerWDesktopBackend final : public IDesktopBackend {
public:
    std::wstring_view Name() const noexcept override { return L"WorkerW (experimental)"; }

    bool Attach(HWND host, DesktopTargetBounds target) override {
        if (!host || !IsWindow(host)) return false;
        HWND worker = FindWorkerWindow();
        if (!worker) return false;

        if (!attached_) {
            originalParent_ = GetParent(host);
            originalStyle_ = GetWindowLongPtrW(host, GWL_STYLE);
            originalExtendedStyle_ = GetWindowLongPtrW(host, GWL_EXSTYLE);
            GetWindowRect(host, &originalBounds_);
        }
        SetLastError(ERROR_SUCCESS);
        if (!SetParent(host, worker) && GetLastError() != ERROR_SUCCESS) return false;
        attached_ = true;
        SetWindowLongPtrW(host, GWL_STYLE,
            (originalStyle_ & ~(WS_OVERLAPPEDWINDOW | WS_POPUP)) | WS_CHILD | WS_VISIBLE);
        SetWindowLongPtrW(host, GWL_EXSTYLE,
            GetWindowLongPtrW(host, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
        RECT bounds{};
        if (!GetClientRect(worker, &bounds)) { Detach(host); return false; }
        POINT origin{};
        int width = bounds.right;
        int height = bounds.bottom;
        if (target.valid) {
            origin = POINT{target.x, target.y};
            if (!ScreenToClient(worker, &origin)) { Detach(host); return false; }
            width = target.width;
            height = target.height;
        }
        if (!SetWindowPos(host, HWND_BOTTOM, origin.x, origin.y, width, height,
                SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_SHOWWINDOW)) {
            Detach(host);
            return false;
        }
        return true;
    }

    void Detach(HWND host) noexcept override {
        if (!attached_ || !host || !IsWindow(host)) return;
        SetParent(host, originalParent_);
        SetWindowLongPtrW(host, GWL_STYLE, originalStyle_);
        SetWindowLongPtrW(host, GWL_EXSTYLE, originalExtendedStyle_);
        SetWindowPos(host, nullptr, originalBounds_.left, originalBounds_.top,
            originalBounds_.right - originalBounds_.left,
            originalBounds_.bottom - originalBounds_.top,
            SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
        attached_ = false;
    }

private:
    static HWND FindWorkerWindow() {
        HWND programManager = FindWindowW(L"Progman", nullptr);
        if (programManager) {
            DWORD_PTR ignored{};
            SendMessageTimeoutW(programManager, 0x052C, 0, 0,
                SMTO_ABORTIFHUNG | SMTO_NORMAL, 1000, &ignored);
        }
        struct Search { HWND worker{}; } search;
        EnumWindows([](HWND top, LPARAM context) -> BOOL {
            auto& result = *reinterpret_cast<Search*>(context);
            if (!FindWindowExW(top, nullptr, L"SHELLDLL_DefView", nullptr)) return TRUE;
            result.worker = FindWindowExW(nullptr, top, L"WorkerW", nullptr);
            return result.worker ? FALSE : TRUE;
        }, reinterpret_cast<LPARAM>(&search));
        return search.worker;
    }

    HWND originalParent_{};
    LONG_PTR originalStyle_{};
    LONG_PTR originalExtendedStyle_{};
    RECT originalBounds_{};
    bool attached_{false};
};

bool WindowedRequested() {
    std::array<wchar_t, 32> value{};
    const DWORD length = GetEnvironmentVariableW(
        L"WIDGETSTUDIO_DESKTOP_BACKEND", value.data(), static_cast<DWORD>(value.size()));
    return length > 0 && length < static_cast<DWORD>(value.size()) &&
        _wcsicmp(value.data(), L"windowed") == 0;
}

} // namespace

bool DesktopBackendController::AttachConfigured(HWND host, DesktopTargetBounds target) {
    experimental_ = false;
    target_ = target;
    if (!WindowedRequested()) {
        auto worker = std::make_unique<WorkerWDesktopBackend>();
        if (worker->Attach(host, target_)) {
            backend_ = std::move(worker);
            experimental_ = true;
            return true;
        }
    }
    backend_ = std::make_unique<WindowedDesktopBackend>();
    return backend_->Attach(host, target_);
}

bool DesktopBackendController::UpdateTarget(HWND host, DesktopTargetBounds target) {
    target_ = target;
    return backend_ ? backend_->Attach(host, target_) : AttachConfigured(host, target_);
}

void DesktopBackendController::Reattach(HWND host) {
    if (!experimental_) return;
    backend_->Detach(host);
    if (!backend_->Attach(host, target_)) AttachConfigured(host, target_);
}

void DesktopBackendController::Detach(HWND host) noexcept {
    if (backend_) backend_->Detach(host);
    backend_.reset();
    experimental_ = false;
}

std::wstring_view DesktopBackendController::ActiveName() const noexcept {
    return backend_ ? backend_->Name() : L"None";
}

} // namespace ws
