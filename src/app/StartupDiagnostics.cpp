#include "app/StartupDiagnostics.h"
#include "Version.h"

#include <array>
#include <cwchar>
#include <filesystem>
#include <system_error>
#include <vector>

namespace ws {
namespace {

std::filesystem::path ExecutableDirectory() noexcept {
    try {
        std::wstring buffer(512, L'\0');
        while (buffer.size() <= 32768) {
            const DWORD length = GetModuleFileNameW(
                nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0) break;
            if (length < buffer.size() - 1) {
                buffer.resize(length);
                return std::filesystem::path(buffer).parent_path();
            }
            buffer.resize(buffer.size() * 2);
        }
    } catch (...) {
    }
    return {};
}

std::filesystem::path PreferredLogDirectory() noexcept {
    try {
        const std::filesystem::path executableDirectory = ExecutableDirectory();
        std::error_code error;
        if (!executableDirectory.empty() &&
            std::filesystem::exists(executableDirectory / L"portable.mode", error) && !error) {
            return executableDirectory / L"data" / L"logs";
        }

        std::wstring localAppData(32768, L'\0');
        const DWORD length = GetEnvironmentVariableW(
            L"LOCALAPPDATA", localAppData.data(), static_cast<DWORD>(localAppData.size()));
        if (length > 0 && length < localAppData.size()) {
            localAppData.resize(length);
            return std::filesystem::path(localAppData) / L"WidgetStudio" / L"logs";
        }
        if (!executableDirectory.empty()) return executableDirectory / L"data" / L"logs";
    } catch (...) {
    }
    return {};
}

std::wstring WindowsMessage(DWORD error) {
    std::array<wchar_t, 512> buffer{};
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
    if (length == 0) return L"no system message";
    std::wstring message(buffer.data(), length);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) {
        message.pop_back();
    }
    return message;
}

} // namespace

StartupDiagnostics::StartupDiagnostics() noexcept {
    Open();
    Log(std::wstring(L"WidgetStudio startup ") + kDisplayVersion);
    try {
        Log(L"process id=" + std::to_wstring(GetCurrentProcessId()));
        using RtlGetVersionFunction = LONG(WINAPI*)(OSVERSIONINFOW*);
        const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFunction>(
            ntdll ? GetProcAddress(ntdll, "RtlGetVersion") : nullptr);
        OSVERSIONINFOW version{};
        version.dwOSVersionInfoSize = sizeof(version);
        if (rtlGetVersion && rtlGetVersion(&version) == 0) {
            Log(L"Windows version=" + std::to_wstring(version.dwMajorVersion) + L"." +
                std::to_wstring(version.dwMinorVersion) + L" build " +
                std::to_wstring(version.dwBuildNumber));
        }
    } catch (...) {
    }
}

StartupDiagnostics::~StartupDiagnostics() {
    if (Available()) CloseHandle(file_);
}

void StartupDiagnostics::Open() noexcept {
    try {
        std::filesystem::path directory = PreferredLogDirectory();
        std::error_code error;
        if (!directory.empty()) std::filesystem::create_directories(directory, error);
        if (!error && !directory.empty()) {
            path_ = (directory / L"startup.log").wstring();
            file_ = CreateFileW(path_.c_str(), GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        }
        if (Available()) return;

        std::wstring temporary(32768, L'\0');
        const DWORD length = GetTempPathW(static_cast<DWORD>(temporary.size()), temporary.data());
        if (length == 0 || length >= temporary.size()) return;
        temporary.resize(length);
        path_ = (std::filesystem::path(temporary) / L"WidgetStudio-startup.log").wstring();
        file_ = CreateFileW(path_.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    } catch (...) {
        file_ = INVALID_HANDLE_VALUE;
        path_.clear();
    }
}

void StartupDiagnostics::WriteUtf8(std::wstring_view message) noexcept {
    if (!Available()) return;
    const int byteCount = WideCharToMultiByte(CP_UTF8, 0, message.data(),
        static_cast<int>(message.size()), nullptr, 0, nullptr, nullptr);
    if (byteCount <= 0) return;
    try {
        std::vector<char> utf8(static_cast<std::size_t>(byteCount) + 2);
        const int converted = WideCharToMultiByte(CP_UTF8, 0, message.data(),
            static_cast<int>(message.size()), utf8.data(), byteCount, nullptr, nullptr);
        if (converted <= 0) return;
        utf8[static_cast<std::size_t>(converted)] = '\r';
        utf8[static_cast<std::size_t>(converted) + 1] = '\n';
        DWORD written{};
        static_cast<void>(WriteFile(file_, utf8.data(), static_cast<DWORD>(converted + 2),
            &written, nullptr));
        static_cast<void>(FlushFileBuffers(file_));
    } catch (...) {
    }
}

void StartupDiagnostics::Log(std::wstring_view message) noexcept {
    try {
        SYSTEMTIME time{};
        GetSystemTime(&time);
        wchar_t prefix[32]{};
        swprintf_s(prefix, L"[%02u:%02u:%02u.%03u] ",
            time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
        std::wstring line(prefix);
        line.append(message);
        WriteUtf8(line);
    } catch (...) {
    }
}

void StartupDiagnostics::LogWin32Failure(std::wstring_view subsystem, DWORD error) noexcept {
    try {
        std::wstring line(subsystem);
        line += L" failed: GetLastError=" + std::to_wstring(error) + L" (0x";
        wchar_t hex[16]{};
        swprintf_s(hex, L"%08lX", error);
        line += hex;
        line += L"), " + WindowsMessage(error);
        Log(line);
    } catch (...) {
    }
}

void StartupDiagnostics::LogHResultFailure(
    std::wstring_view subsystem, HRESULT result) noexcept {
    try {
        std::wstring line(subsystem);
        wchar_t hex[16]{};
        swprintf_s(hex, L"%08lX", static_cast<unsigned long>(result));
        line += L" failed: HRESULT=0x";
        line += hex;
        line += L", " + WindowsMessage(static_cast<DWORD>(result));
        Log(line);
    } catch (...) {
    }
}

} // namespace ws
