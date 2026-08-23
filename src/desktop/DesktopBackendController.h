#pragma once

#include "desktop/IDesktopBackend.h"

#include <memory>
#include <string>

namespace ws {

class DesktopBackendController {
public:
    bool AttachConfigured(HWND host);
    void Reattach(HWND host);
    void Detach(HWND host) noexcept;
    [[nodiscard]] std::wstring_view ActiveName() const noexcept;
    [[nodiscard]] bool IsExperimental() const noexcept { return experimental_; }

private:
    std::unique_ptr<IDesktopBackend> backend_;
    bool experimental_{false};
};

} // namespace ws
