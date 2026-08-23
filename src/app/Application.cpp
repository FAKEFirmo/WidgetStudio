#include "app/Application.h"
#include "desktop/DesktopHost.h"
#include "widgets/DebugWidget.h"
#include "widgets/WidgetRegistry.h"

#include <objbase.h>

namespace ws {

int Application::Run(HINSTANCE instance, int showCommand) {
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
        WidgetRegistry registry;
        RegisterBuiltInWidgets(registry);
        if (registry.Descriptors().empty()) {
            MessageBoxW(nullptr, L"Widget Studio has no registered widget types.",
                        L"Widget Studio", MB_OK | MB_ICONERROR);
            if (comInitialized) CoUninitialize();
            return 1;
        }
        DesktopHost host(registry);
        if (!host.Create(instance, showCommand)) {
            MessageBoxW(nullptr, L"Widget Studio could not initialize.",
                        L"Widget Studio", MB_OK | MB_ICONERROR);
        } else {
            result = host.RunMessageLoop();
        }
    }

    if (comInitialized) {
        CoUninitialize();
    }
    return result;
}

} // namespace ws
