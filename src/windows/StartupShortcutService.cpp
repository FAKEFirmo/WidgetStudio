#include "windows/StartupShortcutService.h"

#include <filesystem>
#include <shlobj.h>
#include <shobjidl.h>
#include <windows.h>
#include <wrl/client.h>

namespace ws {
namespace {

constexpr wchar_t kShortcutName[] = L"WidgetStudio.lnk";

std::wstring WindowsErrorMessage(DWORD error) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::wstring message = length && buffer ? std::wstring(buffer, length) : L"Windows error " + std::to_wstring(error);
    if (buffer) LocalFree(buffer);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) message.pop_back();
    return message;
}

bool ExecutablePath(std::wstring& path, std::wstring& errorMessage) {
    std::wstring buffer(260, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            errorMessage = WindowsErrorMessage(GetLastError());
            return false;
        }
        if (length < buffer.size() - 1) {
            buffer.resize(length);
            path = std::move(buffer);
            return true;
        }
        buffer.resize(buffer.size() * 2);
    }
}

} // namespace

bool StartupShortcutService::ShortcutPath(std::wstring& path, std::wstring& errorMessage) {
    PWSTR startupFolder = nullptr;
    const HRESULT result = SHGetKnownFolderPath(FOLDERID_Startup, KF_FLAG_DEFAULT, nullptr, &startupFolder);
    if (FAILED(result)) {
        errorMessage = L"Could not locate the Startup folder (HRESULT " + std::to_wstring(result) + L").";
        return false;
    }
    path = (std::filesystem::path(startupFolder) / kShortcutName).wstring();
    CoTaskMemFree(startupFolder);
    return true;
}

bool StartupShortcutService::IsEnabled() const noexcept {
    std::wstring path;
    std::wstring ignored;
    return ShortcutPath(path, ignored) && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool StartupShortcutService::SetEnabled(bool enabled, std::wstring& errorMessage) const {
    std::wstring shortcutPath;
    if (!ShortcutPath(shortcutPath, errorMessage)) return false;

    if (!enabled) {
        if (DeleteFileW(shortcutPath.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) return true;
        errorMessage = L"Could not remove the Startup shortcut: " + WindowsErrorMessage(GetLastError());
        return false;
    }

    std::wstring executablePath;
    if (!ExecutablePath(executablePath, errorMessage)) return false;

    Microsoft::WRL::ComPtr<IShellLinkW> shellLink;
    HRESULT result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink));
    if (SUCCEEDED(result)) result = shellLink->SetPath(executablePath.c_str());
    const std::wstring workingDirectory = std::filesystem::path(executablePath).parent_path().wstring();
    if (SUCCEEDED(result)) result = shellLink->SetWorkingDirectory(workingDirectory.c_str());
    if (SUCCEEDED(result)) result = shellLink->SetDescription(L"Launch WidgetStudio at sign-in");

    Microsoft::WRL::ComPtr<IPersistFile> persistFile;
    if (SUCCEEDED(result)) result = shellLink.As(&persistFile);
    if (SUCCEEDED(result)) result = persistFile->Save(shortcutPath.c_str(), TRUE);
    if (FAILED(result)) {
        errorMessage = L"Could not create the Startup shortcut (HRESULT " + std::to_wstring(result) + L").";
        return false;
    }
    return true;
}

} // namespace ws
