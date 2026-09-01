#pragma once

#include <string>
#include <string_view>
#include <windows.h>

namespace ws {

// Temporary local-only trace used to diagnose pre-message-loop exits. It never
// transmits data and may be removed once physical-machine startup is validated.
class StartupDiagnostics {
public:
    StartupDiagnostics() noexcept;
    ~StartupDiagnostics();
    StartupDiagnostics(const StartupDiagnostics&) = delete;
    StartupDiagnostics& operator=(const StartupDiagnostics&) = delete;

    void Log(std::wstring_view message) noexcept;
    void LogWin32Failure(std::wstring_view subsystem, DWORD error) noexcept;
    void LogHResultFailure(std::wstring_view subsystem, HRESULT result) noexcept;

    [[nodiscard]] const std::wstring& Path() const noexcept { return path_; }
    [[nodiscard]] bool Available() const noexcept {
        return file_ != nullptr && file_ != INVALID_HANDLE_VALUE;
    }

private:
    void Open() noexcept;
    void WriteUtf8(std::wstring_view message) noexcept;

    HANDLE file_{INVALID_HANDLE_VALUE};
    std::wstring path_;
};

} // namespace ws
