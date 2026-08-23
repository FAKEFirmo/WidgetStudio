#pragma once

#include <map>
#include <span>
#include <string>

namespace ws {

struct WidgetRenderContext;

using WidgetState = std::map<std::wstring, std::wstring>;

enum class WidgetSettingKind { Boolean, Number, Text, Choice };

struct WidgetSettingDefinition {
    std::wstring key;
    std::wstring displayName;
    WidgetSettingKind kind{WidgetSettingKind::Text};
};

class IWidget {
public:
    virtual ~IWidget() = default;
    virtual void Render(const WidgetRenderContext& context) const = 0;
    [[nodiscard]] virtual std::span<const WidgetSettingDefinition> Settings() const noexcept = 0;
    [[nodiscard]] virtual WidgetState SaveState() const = 0;
    virtual void RestoreState(const WidgetState& state) = 0;
};

} // namespace ws
