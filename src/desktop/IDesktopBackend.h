#pragma once

#include <string_view>
#include <windows.h>

namespace ws {

struct DesktopTargetBounds {
    int x{};
    int y{};
    int width{};
    int height{};
    bool valid{false};
};

class IDesktopBackend {
public:
    virtual ~IDesktopBackend() = default;
    [[nodiscard]] virtual std::wstring_view Name() const noexcept = 0;
    virtual bool Attach(HWND host, DesktopTargetBounds target) = 0;
    virtual void Detach(HWND host) noexcept = 0;
};

} // namespace ws
