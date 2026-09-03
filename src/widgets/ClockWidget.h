#pragma once

#include "widgets/IWidget.h"
#include "widgets/WidgetDescriptor.h"

#include <dwrite.h>
#include <wrl/client.h>

namespace ws {

class ClockWidget final : public IWidget {
public:
    void Render(const WidgetRenderContext& context) const override;
    [[nodiscard]] std::span<const WidgetSettingDefinition> Settings() const noexcept override;
    [[nodiscard]] WidgetState SaveState() const override;
    void RestoreState(const WidgetState& state) override;
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> NextUpdateTime() const noexcept override;

    [[nodiscard]] static WidgetDescriptor Descriptor();

private:
    HRESULT EnsureTextFormats(IDWriteFactory& factory, std::wstring_view fontFamily) const;

    bool use24Hour_{true};
    bool showSeconds_{false};
    bool showDivider_{true};
    bool showDate_{true};
    std::wstring dateFormat_{L"long"};
    mutable std::wstring formatFontFamily_;
    mutable Microsoft::WRL::ComPtr<IDWriteTextFormat> timeFormat_;
    mutable Microsoft::WRL::ComPtr<IDWriteTextFormat> dateTextFormat_;
};

} // namespace ws
