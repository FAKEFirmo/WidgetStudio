#include "app/Application.h"
#include "desktop/DesktopHost.h"
#include "widgets/BuiltInWidgets.h"
#include "widgets/WidgetRegistry.h"
#include "windows/MediaSessionService.h"

#include <memory>
#include <objbase.h>

namespace ws {
namespace {

struct HandleCloser {
    void operator()(void* handle) const noexcept {
        if (handle) CloseHandle(handle);
    }
};

} // namespace

int Application::Run(HINSTANCE instance, int showCommand) {
    const HANDLE instanceMutex = CreateMutexW(nullptr, FALSE, L"Local\\WidgetStudio.SingleInstance");
    const DWORD instanceMutexError = GetLastError();
    if (!instanceMutex) {
        MessageBoxW(nullptr, L"Widget Studio could not create its single-instance guard.",
            L"Widget Studio", MB_OK | MB_ICONERROR);
        return 1;
    }
    const std::unique_ptr<void, HandleCloser> instanceGuard(instanceMutex);
    if (instanceMutexError == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    // The app is designed for correct behavior when a window crosses monitors
    // with different scale factors.
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        const DWORD error = GetLastError();
        const bool alreadyPerMonitorV2 = error == ERROR_ACCESS_DENIED &&
            AreDpiAwarenessContextsEqual(
                GetThreadDpiAwarenessContext(),
                DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        if (!alreadyPerMonitorV2) {
            MessageBoxW(nullptr, L"Widget Studio could not enable per-monitor DPI awareness.",
                        L"Widget Studio", MB_OK | MB_ICONERROR);
            return 1;
        }
    }

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comInitialized = SUCCEEDED(comResult);
    if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE) {
        MessageBoxW(nullptr, L"Widget Studio could not initialize COM.",
                    L"Widget Studio", MB_OK | MB_ICONERROR);
        return 1;
    }

    int result = 1;
    {
        // Keep every COM-owning subsystem inside this scope so its interfaces
        // are released before the matching CoUninitialize call.
        auto mediaSession = std::make_shared<MediaSessionService>();
        WidgetRegistry registry;
        if (!RegisterBuiltInWidgets(registry, mediaSession)) {
            MessageBoxW(nullptr, L"Widget Studio could not register its built-in widget types.",
                        L"Widget Studio", MB_OK | MB_ICONERROR);
        } else {
            DesktopHost host(registry, mediaSession);
            if (!host.Create(instance, showCommand)) {
                MessageBoxW(nullptr, L"Widget Studio could not initialize.",
                            L"Widget Studio", MB_OK | MB_ICONERROR);
            } else {
                result = host.RunMessageLoop();
            }
        }
    }

    if (comInitialized) {
        CoUninitialize();
    }
    return result;
}

} // namespace ws
