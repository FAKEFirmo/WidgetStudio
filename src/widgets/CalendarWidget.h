#pragma once

#include "widgets/IWidget.h"
#include "widgets/WidgetDescriptor.h"

#include <dwrite.h>
#include <wrl/client.h>

namespace ws {

class CalendarWidget final : public IWidget {
public:
    void Render(const WidgetRenderContext& context) const override;
    [[nodiscard]] std::span<const WidgetSettingDefinition> Settings() const noexcept override;
    [[nodiscard]] WidgetState SaveState() const override;
    void RestoreState(const WidgetState& state) override;
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> NextUpdateTime() const noexcept override;

    [[nodiscard]] static WidgetDescriptor Descriptor();

private:
    HRESULT EnsureTextFormats(IDWriteFactory& factory) const;

    bool mondayFirst_{true};
    bool dimWeekends_{false};
    mutable Microsoft::WRL::ComPtr<IDWriteTextFormat> monthFormat_;
    mutable Microsoft::WRL::ComPtr<IDWriteTextFormat> yearFormat_;
    mutable Microsoft::WRL::ComPtr<IDWriteTextFormat> weekdayFormat_;
    mutable Microsoft::WRL::ComPtr<IDWriteTextFormat> dayFormat_;
};

} // namespace ws
