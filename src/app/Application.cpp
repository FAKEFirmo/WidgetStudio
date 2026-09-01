#include "app/Application.h"
#include "app/StartupDiagnostics.h"
#include "desktop/DesktopHost.h"
#include "widgets/BuiltInWidgets.h"
#include "widgets/WidgetRegistry.h"
#include "windows/MediaSessionService.h"

#include <memory>
#include <objbase.h>
#include <shellscalingapi.h>

namespace ws {
namespace {

struct HandleCloser {
    void operator()(void* handle) const noexcept {
        if (handle) CloseHandle(handle);
    }
};

int FailStartup(StartupDiagnostics& diagnostics, int exitCode,
    std::wstring_view subsystem, std::wstring_view userMessage) {
    diagnostics.Log(std::wstring(L"startup failed: ") + std::wstring(subsystem) +
        L"; exit code=" + std::to_wstring(exitCode));
    std::wstring message(userMessage);
    message += L"\n\nStartup log: ";
    message += diagnostics.Path().empty() ? L"unavailable" : diagnostics.Path();
    MessageBoxW(nullptr, message.c_str(), L"Widget Studio", MB_OK | MB_ICONERROR);
    return exitCode;
}

bool InitializeDpi(StartupDiagnostics& diagnostics) {
    using SetDpiContext = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    using GetThreadDpiContext = DPI_AWARENESS_CONTEXT(WINAPI*)();
    using EqualDpiContexts = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT, DPI_AWARENESS_CONTEXT);

    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    const auto setContext = reinterpret_cast<SetDpiContext>(
        user32 ? GetProcAddress(user32, "SetProcessDpiAwarenessContext") : nullptr);
    const auto getThreadContext = reinterpret_cast<GetThreadDpiContext>(
        user32 ? GetProcAddress(user32, "GetThreadDpiAwarenessContext") : nullptr);
    const auto equalContexts = reinterpret_cast<EqualDpiContexts>(
        user32 ? GetProcAddress(user32, "AreDpiAwarenessContextsEqual") : nullptr);

    if (setContext) {
        SetLastError(ERROR_SUCCESS);
        if (setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            diagnostics.Log(L"DPI initialized: Per-Monitor V2 API");
            return true;
        }
        const DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED && getThreadContext && equalContexts &&
            equalContexts(getThreadContext(), DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            diagnostics.Log(L"DPI initialized: Per-Monitor V2 manifest context already active");
            return true;
        }
        diagnostics.LogWin32Failure(L"SetProcessDpiAwarenessContext", error);
    } else {
        diagnostics.Log(L"Per-Monitor V2 DPI API unavailable; trying Windows 8.1 fallback");
    }

    const HRESULT fallback = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    if (SUCCEEDED(fallback)) {
        diagnostics.Log(L"DPI initialized: per-monitor fallback");
        return true;
    }
    PROCESS_DPI_AWARENESS current{};
    if (fallback == E_ACCESSDENIED &&
        SUCCEEDED(GetProcessDpiAwareness(nullptr, &current)) &&
        current == PROCESS_PER_MONITOR_DPI_AWARE) {
        diagnostics.Log(L"DPI initialized: per-monitor manifest context already active");
        return true;
    }
    diagnostics.LogHResultFailure(L"SetProcessDpiAwareness fallback", fallback);
    return false;
}

void LogExistingInstance(StartupDiagnostics& diagnostics) {
    const HWND existingHost = FindWindowW(L"WidgetStudioHostWindow", L"WidgetStudio");
    if (!existingHost) {
        diagnostics.Log(L"existing instance host window was not found");
        return;
    }
    DWORD processId{};
    GetWindowThreadProcessId(existingHost, &processId);
    diagnostics.Log(L"existing instance process id=" + std::to_wstring(processId));
    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) {
        diagnostics.LogWin32Failure(L"OpenProcess(existing WidgetStudio)", GetLastError());
        return;
    }
    std::wstring path(32768, L'\0');
    DWORD pathLength = static_cast<DWORD>(path.size());
    if (QueryFullProcessImageNameW(process, 0, path.data(), &pathLength)) {
        path.resize(pathLength);
        diagnostics.Log(L"existing instance path=" + path);
    } else {
        diagnostics.LogWin32Failure(L"QueryFullProcessImageName(existing WidgetStudio)", GetLastError());
    }
    CloseHandle(process);
}

} // namespace

int Application::Run(HINSTANCE instance, int showCommand, StartupDiagnostics& diagnostics) {
    diagnostics.Log(L"application initialization entered");
    SetLastError(ERROR_SUCCESS);
    const HANDLE instanceMutex = CreateMutexW(nullptr, FALSE, L"Local\\WidgetStudio.SingleInstance");
    const DWORD instanceMutexError = GetLastError();
    if (!instanceMutex) {
        diagnostics.LogWin32Failure(L"CreateMutex(single-instance guard)", instanceMutexError);
        return FailStartup(diagnostics, 10, L"single-instance guard",
            L"Widget Studio could not create its single-instance guard.");
    }
    const std::unique_ptr<void, HandleCloser> instanceGuard(instanceMutex);
    if (instanceMutexError == ERROR_ALREADY_EXISTS) {
        diagnostics.Log(L"single-instance guard already exists; exiting with code 2");
        LogExistingInstance(diagnostics);
        return 2;
    }
    diagnostics.Log(L"single-instance guard acquired");

    // The app is designed for correct behavior when a window crosses monitors
    // with different scale factors.
    if (!InitializeDpi(diagnostics)) {
        return FailStartup(diagnostics, 11, L"DPI initialization",
            L"Widget Studio could not enable per-monitor DPI awareness.");
    }

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comInitialized = SUCCEEDED(comResult);
    if (FAILED(comResult)) {
        diagnostics.LogHResultFailure(L"CoInitializeEx(STA)", comResult);
        return FailStartup(diagnostics, 12, L"COM initialization",
            L"Widget Studio could not initialize COM.");
    }
    diagnostics.Log(L"COM initialized: apartment-threaded");

    int result = 1;
    {
        // Keep every COM-owning subsystem inside this scope so its interfaces
        // are released before the matching CoUninitialize call.
        auto mediaSession = std::make_shared<MediaSessionService>();
        diagnostics.Log(L"media-session service allocated (lazy initialization)");
        WidgetRegistry registry;
        if (!RegisterBuiltInWidgets(registry, mediaSession)) {
            result = FailStartup(diagnostics, 13, L"widget registry",
                L"Widget Studio could not register its built-in widget types.");
        } else {
            diagnostics.Log(L"registry initialized: " +
                std::to_wstring(registry.Descriptors().size()) + L" widget types");
            diagnostics.Log(L"widget library initialized: deferred native window");
            // First-run product policy belongs in the composition root. The
            // host remains generic and never branches on concrete type IDs.
            DesktopHost host(registry, mediaSession, diagnostics, "clock");
            if (!host.Create(instance, showCommand)) {
                result = FailStartup(diagnostics, 14, L"desktop host",
                    L"Widget Studio could not initialize its desktop host.");
            } else {
                result = host.RunMessageLoop();
            }
        }
    }

    if (comInitialized) {
        CoUninitialize();
        diagnostics.Log(L"COM uninitialized");
    }
    diagnostics.Log(L"WidgetStudio exiting with code " + std::to_wstring(result));
    return result;
}

} // namespace ws
