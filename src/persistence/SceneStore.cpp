#include "persistence/SceneStore.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <shlobj.h>
#include <system_error>
#include <utility>
#include <windows.h>

namespace ws {
namespace {

constexpr LONGLONG kMaximumConfigBytes = 16LL * 1024LL * 1024LL;

class UniqueHandle {
public:
    explicit UniqueHandle(HANDLE handle = INVALID_HANDLE_VALUE) noexcept : handle_(handle) {}
    ~UniqueHandle() { Reset(); }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.Release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) { Reset(); handle_ = other.Release(); }
        return *this;
    }
    [[nodiscard]] HANDLE Get() const noexcept { return handle_; }
    [[nodiscard]] bool Valid() const noexcept { return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr; }
    HANDLE Release() noexcept { const HANDLE value = handle_; handle_ = INVALID_HANDLE_VALUE; return value; }
    void Reset() noexcept {
        if (Valid()) CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }

private:
    HANDLE handle_;
};

std::wstring WindowsErrorMessage(DWORD error) {
    std::array<wchar_t, 512> buffer{};
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
    if (length == 0) return L"Windows error " + std::to_wstring(error);
    std::wstring message(buffer.data(), length);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) message.pop_back();
    return message;
}

std::filesystem::path ExecutableDirectory() {
    std::wstring buffer(512, L'\0');
    while (true) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) return std::filesystem::current_path();
        if (static_cast<std::size_t>(length) < buffer.size() - 1) {
            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
}

} // namespace

SceneStore::SceneStore(std::filesystem::path configPath) : configPath_(std::move(configPath)) {}

std::filesystem::path SceneStore::DefaultConfigPath() {
    const std::filesystem::path executableDirectory = ExecutableDirectory();
    std::error_code error;
    if (std::filesystem::exists(executableDirectory / L"portable.mode", error)) {
        return executableDirectory / L"portable-data" / L"scene.json";
    }

    PWSTR localAppData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &localAppData))) {
        std::filesystem::path path(localAppData);
        CoTaskMemFree(localAppData);
        return path / L"WidgetStudio" / L"scene.json";
    }
    return executableDirectory / L"portable-data" / L"scene.json";
}

SceneLoadResult SceneStore::Load() const {
    UniqueHandle file(CreateFileW(configPath_.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (!file.Valid()) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return SceneLoadResult{.status = SceneLoadStatus::Missing};
        }
        return SceneLoadResult{.status = SceneLoadStatus::IoError, .message = WindowsErrorMessage(error)};
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.Get(), &size)) {
        return SceneLoadResult{.status = SceneLoadStatus::IoError, .message = WindowsErrorMessage(GetLastError())};
    }
    if (size.QuadPart < 0 || size.QuadPart > kMaximumConfigBytes) {
        return SceneLoadResult{.status = SceneLoadStatus::Invalid, .message = L"Scene configuration is too large."};
    }

    std::string json(static_cast<std::size_t>(size.QuadPart), '\0');
    std::size_t offset{};
    while (offset < json.size()) {
        const DWORD remaining = static_cast<DWORD>(std::min<std::size_t>(
            json.size() - offset, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
        DWORD bytesRead{};
        if (!ReadFile(file.Get(), json.data() + offset, remaining, &bytesRead, nullptr)) {
            return SceneLoadResult{.status = SceneLoadStatus::IoError, .message = WindowsErrorMessage(GetLastError())};
        }
        if (bytesRead == 0) {
            return SceneLoadResult{.status = SceneLoadStatus::IoError, .message = L"Unexpected end of scene file."};
        }
        offset += bytesRead;
    }

    std::wstring decodeError;
    std::optional<DecodedScene> decoded = SceneJsonCodec::Decode(json, decodeError);
    if (!decoded) {
        return SceneLoadResult{.status = SceneLoadStatus::Invalid, .message = std::move(decodeError)};
    }
    return SceneLoadResult{.status = SceneLoadStatus::Loaded, .snapshot = std::move(decoded->widgets)};
}

bool SceneStore::Save(const WidgetSceneSnapshot& snapshot, std::wstring& errorMessage) const {
    std::error_code directoryError;
    std::filesystem::create_directories(configPath_.parent_path(), directoryError);
    if (directoryError) {
        const std::string message = directoryError.message();
        errorMessage = L"Could not create the configuration directory: " +
            std::wstring(message.begin(), message.end());
        return false;
    }

    std::string json;
    try {
        json = SceneJsonCodec::Encode(snapshot);
    } catch (const std::exception& error) {
        const std::string message = error.what();
        errorMessage.assign(message.begin(), message.end());
        return false;
    }

    const std::filesystem::path temporaryPath = configPath_.wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId());
    const std::filesystem::path backupPath = configPath_.wstring() + L".bak";
    UniqueHandle file(CreateFileW(temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr));
    if (!file.Valid()) {
        errorMessage = WindowsErrorMessage(GetLastError());
        return false;
    }

    std::size_t offset{};
    while (offset < json.size()) {
        const DWORD remaining = static_cast<DWORD>(std::min<std::size_t>(
            json.size() - offset, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
        DWORD bytesWritten{};
        if (!WriteFile(file.Get(), json.data() + offset, remaining, &bytesWritten, nullptr) || bytesWritten == 0) {
            const DWORD error = GetLastError();
            file.Reset();
            DeleteFileW(temporaryPath.c_str());
            errorMessage = WindowsErrorMessage(error);
            return false;
        }
        offset += bytesWritten;
    }
    if (!FlushFileBuffers(file.Get())) {
        const DWORD error = GetLastError();
        file.Reset();
        DeleteFileW(temporaryPath.c_str());
        errorMessage = WindowsErrorMessage(error);
        return false;
    }
    file.Reset();

    const DWORD attributes = GetFileAttributesW(configPath_.c_str());
    bool replaced = false;
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        replaced = ReplaceFileW(configPath_.c_str(), temporaryPath.c_str(), backupPath.c_str(),
            0, nullptr, nullptr) != FALSE;
    } else {
        replaced = MoveFileExW(temporaryPath.c_str(), configPath_.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
    }
    if (!replaced) {
        const DWORD error = GetLastError();
        DeleteFileW(temporaryPath.c_str());
        errorMessage = WindowsErrorMessage(error);
        return false;
    }

    errorMessage.clear();
    return true;
}

} // namespace ws
