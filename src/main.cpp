#include "app/Application.h"
#include "app/StartupDiagnostics.h"

#include <exception>
#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    ws::StartupDiagnostics diagnostics;
    try {
        ws::Application application;
        return application.Run(instance, showCommand, diagnostics);
    } catch (const std::exception&) {
        diagnostics.Log(L"fatal startup exception: std::exception");
    } catch (...) {
        diagnostics.Log(L"fatal startup exception: unknown exception");
    }
    const std::wstring message = L"Widget Studio encountered an unexpected startup failure.\n\nLog: " +
        (diagnostics.Path().empty() ? std::wstring(L"unavailable") : diagnostics.Path());
    MessageBoxW(nullptr, message.c_str(), L"Widget Studio", MB_OK | MB_ICONERROR);
    return 99;
}
