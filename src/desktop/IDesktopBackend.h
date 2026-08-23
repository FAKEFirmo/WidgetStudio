#pragma once

#include <string_view>
#include <windows.h>

namespace ws {

class IDesktopBackend {
public:
    virtual ~IDesktopBackend() = default;
    [[nodiscard]] virtual std::wstring_view Name() const noexcept = 0;
    virtual bool Attach(HWND host) = 0;
    virtual void Detach(HWND host) noexcept = 0;
};

} // namespace ws
