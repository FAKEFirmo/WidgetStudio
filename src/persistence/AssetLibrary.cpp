#include "persistence/AssetLibrary.h"

#include <array>
#include <chrono>
#include <system_error>
#include <windows.h>

namespace ws {
namespace {

std::wstring WindowsErrorMessage(DWORD error) {
    std::array<wchar_t, 512> buffer{};
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
    if (length == 0) return L"Windows error " + std::to_wstring(error);
    std::wstring message(buffer.data(), length);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) message.pop_back();
    return message;
}

} // namespace

std::optional<std::filesystem::path> AssetLibrary::Import(
    const std::filesystem::path& source, std::wstring& errorMessage) const {
    const DWORD attributes = GetFileAttributesW(source.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        errorMessage = L"The selected image file does not exist.";
        return std::nullopt;
    }

    std::error_code directoryError;
    std::filesystem::create_directories(directory_, directoryError);
    if (directoryError) {
        const std::string message = directoryError.message();
        errorMessage = std::wstring(message.begin(), message.end());
        return std::nullopt;
    }

    std::wstring extension = source.extension().wstring();
    if (extension.size() > 12) extension.clear();
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::wstring fileName = L"asset-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
        std::to_wstring(ticks) + extension;
    const std::filesystem::path destination = directory_ / fileName;
    const std::filesystem::path temporary = destination.wstring() + L".tmp";

    if (!CopyFileW(source.c_str(), temporary.c_str(), TRUE)) {
        errorMessage = WindowsErrorMessage(GetLastError());
        return std::nullopt;
    }
    if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        DeleteFileW(temporary.c_str());
        errorMessage = WindowsErrorMessage(error);
        return std::nullopt;
    }
    errorMessage.clear();
    return destination;
}

std::wstring AssetLibrary::ReferenceFor(const std::filesystem::path& importedPath) const {
    if (importedPath.parent_path() == directory_ && !importedPath.filename().empty()) {
        return L"asset://" + importedPath.filename().wstring();
    }
    return importedPath.wstring();
}

} // namespace ws
