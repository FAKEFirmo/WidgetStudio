#pragma once

#include "persistence/WidgetPersistence.h"

#include <optional>
#include <string>
#include <string_view>

namespace ws {

struct DecodedScene {
    int schemaVersion{};
    WidgetAppearance generalAppearance{};
    WidgetSceneSnapshot widgets;
};

class SceneJsonCodec {
public:
    static constexpr int kCurrentSchemaVersion = 2;

    [[nodiscard]] static std::string Encode(const WidgetSceneSnapshot& snapshot);
    [[nodiscard]] static std::optional<DecodedScene> Decode(
        std::string_view json, std::wstring& errorMessage) noexcept;
};

} // namespace ws
