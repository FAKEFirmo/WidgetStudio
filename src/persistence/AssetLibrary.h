#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace ws {

class AssetLibrary {
public:
    explicit AssetLibrary(std::filesystem::path directory) : directory_(std::move(directory)) {}

    [[nodiscard]] const std::filesystem::path& Directory() const noexcept { return directory_; }
    [[nodiscard]] std::optional<std::filesystem::path> Import(
        const std::filesystem::path& source, std::wstring& errorMessage) const;

private:
    std::filesystem::path directory_;
};

} // namespace ws
