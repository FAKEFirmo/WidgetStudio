#pragma once

#include <string>

namespace ws {

class StartupShortcutService {
public:
    [[nodiscard]] bool IsEnabled() const noexcept;
    bool SetEnabled(bool enabled, std::wstring& errorMessage) const;

private:
    [[nodiscard]] static bool ShortcutPath(std::wstring& path, std::wstring& errorMessage);
};

} // namespace ws
