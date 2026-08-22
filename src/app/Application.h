#pragma once

#include <windows.h>

namespace ws {

class Application {
public:
    int Run(HINSTANCE instance, int showCommand);
};

} // namespace ws
