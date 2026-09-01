#pragma once

#include <windows.h>

namespace ws {

class StartupDiagnostics;

class Application {
public:
    int Run(HINSTANCE instance, int showCommand, StartupDiagnostics& diagnostics);
};

} // namespace ws
