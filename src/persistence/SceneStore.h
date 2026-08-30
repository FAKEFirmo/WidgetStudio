#pragma once

#include "persistence/SceneJsonCodec.h"

#include <filesystem>
#include <string>

namespace ws {

enum class SceneLoadStatus {
    Loaded,
    Missing,
    Invalid,
    IoError,
};

struct SceneLoadResult {
    SceneLoadStatus status{SceneLoadStatus::Missing};
    WidgetSceneSnapshot snapshot;
    std::wstring message;
};

class SceneStore {
public:
    explicit SceneStore(std::filesystem::path configPath);

    [[nodiscard]] static std::filesystem::path DefaultDataDirectory();
    [[nodiscard]] static std::filesystem::path DefaultConfigPath();
    [[nodiscard]] static std::filesystem::path DefaultImageDirectory();
    [[nodiscard]] static std::filesystem::path DefaultCacheDirectory();
    [[nodiscard]] const std::filesystem::path& ConfigPath() const noexcept { return configPath_; }
    [[nodiscard]] SceneLoadResult Load() const;
    [[nodiscard]] bool Save(const WidgetSceneSnapshot& snapshot, std::wstring& errorMessage) const;

private:
    std::filesystem::path configPath_;
};

} // namespace ws
