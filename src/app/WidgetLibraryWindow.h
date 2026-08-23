#pragma once

#include <functional>
#include <string>
#include <windows.h>

namespace ws {

class WidgetRegistry;

class WidgetLibraryWindow {
public:
    WidgetLibraryWindow() = default;
    ~WidgetLibraryWindow();

    bool Open(HWND owner, HINSTANCE instance, const WidgetRegistry& registry,
        std::function<void(std::string)> createWidget);
    void Close() noexcept;

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateSelectedWidget();
    void UpdateDescription();
    void LayoutControls(int width, int height);

    HWND hwnd_{};
    HWND list_{};
    HWND description_{};
    HWND add_{};
    const WidgetRegistry* registry_{};
    std::function<void(std::string)> createWidget_;
};

} // namespace ws
