#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ws {

struct WidgetRenderContext;

using WidgetState = std::map<std::wstring, std::wstring>;

enum class WidgetSettingKind { Boolean, Number, Text, Choice };

struct WidgetSettingDefinition {
    std::wstring key;
    std::wstring displayName;
    WidgetSettingKind kind{WidgetSettingKind::Text};
    std::vector<std::wstring> choices;
    double minimum{};
    double maximum{};
    double step{};
};

class IWidget {
public:
    virtual ~IWidget() = default;
    virtual void Render(const WidgetRenderContext& context) const = 0;
    [[nodiscard]] virtual std::span<const WidgetSettingDefinition> Settings() const noexcept = 0;
    [[nodiscard]] virtual WidgetState SaveState() const = 0;
    virtual void RestoreState(const WidgetState& state) = 0;
    [[nodiscard]] virtual std::optional<std::chrono::system_clock::time_point> NextUpdateTime() const noexcept {
        return std::nullopt;
    }
};

} // namespace ws
