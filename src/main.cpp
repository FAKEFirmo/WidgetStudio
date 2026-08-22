#include "app/Application.h"

#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    ws::Application application;
    return application.Run(instance, showCommand);
}
