#pragma once

#include "common/Geometry.h"

#include <chrono>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ws {

struct WidgetRenderContext;

struct WidgetHitTestContext {
    PointF point{};
    RectF bounds{};
    float contentScale{1.0f};
};

using WidgetState = std::map<std::wstring, std::wstring>;

enum class WidgetSettingKind { Boolean, Number, Text, Choice, File };

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
    [[nodiscard]] virtual std::optional<std::string> HitTestAction(
        const WidgetHitTestContext&) const { return std::nullopt; }
    virtual bool InvokeAction(std::string_view) { return false; }
};

} // namespace ws
