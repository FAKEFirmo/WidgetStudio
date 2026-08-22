#pragma once

#include "desktop/DesktopHost.h"

#include <windows.h>

namespace ws {

class Application {
public:
    int Run(HINSTANCE instance, int showCommand);

private:
    DesktopHost host_{};
};

} // namespace ws
