#pragma once

#include "widgets/IWidget.h"

namespace ws {

class WidgetRegistry;

class DebugWidget final : public IWidget {
public:
    void Render(const WidgetRenderContext& context) const override;
    [[nodiscard]] std::span<const WidgetSettingDefinition> Settings() const noexcept override;
    [[nodiscard]] WidgetState SaveState() const override;
    void RestoreState(const WidgetState& state) override;
};

void RegisterBuiltInWidgets(WidgetRegistry& registry);

} // namespace ws
