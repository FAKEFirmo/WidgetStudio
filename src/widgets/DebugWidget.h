#pragma once

#include "widgets/IWidget.h"
#include "widgets/WidgetDescriptor.h"

namespace ws {

class DebugWidget final : public IWidget {
public:
    void Render(const WidgetRenderContext& context) const override;
    [[nodiscard]] std::span<const WidgetSettingDefinition> Settings() const noexcept override;
    [[nodiscard]] WidgetState SaveState() const override;
    void RestoreState(const WidgetState& state) override;
    [[nodiscard]] static WidgetDescriptor Descriptor();
};

} // namespace ws
