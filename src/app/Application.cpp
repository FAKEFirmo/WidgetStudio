#include "app/Application.h"

#include <objbase.h>

namespace ws {

int Application::Run(HINSTANCE instance, int showCommand) {
    // The app is designed for correct behavior when a window crosses monitors
    // with different scale factors.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comInitialized = SUCCEEDED(comResult);

    if (!host_.Create(instance, showCommand)) {
        if (comInitialized) CoUninitialize();
        MessageBoxW(nullptr, L"Widget Studio could not initialize.", L"Widget Studio", MB_OK | MB_ICONERROR);
        return 1;
    }

    const int result = host_.RunMessageLoop();
    if (comInitialized) CoUninitialize();
    return result;
}

} // namespace ws
