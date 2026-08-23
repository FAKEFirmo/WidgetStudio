#pragma once

#include "widgets/IWidget.h"
#include "widgets/WidgetDescriptor.h"

#include <d2d1.h>
#include <cstdint>
#include <wrl/client.h>

namespace ws {

class PhotoWidget final : public IWidget {
public:
    void Render(const WidgetRenderContext& context) const override;
    [[nodiscard]] std::span<const WidgetSettingDefinition> Settings() const noexcept override;
    [[nodiscard]] WidgetState SaveState() const override;
    void RestoreState(const WidgetState& state) override;

    [[nodiscard]] static WidgetDescriptor Descriptor();

private:
    HRESULT EnsureBitmap(const WidgetRenderContext& context) const;

    std::wstring assetPath_;
    std::wstring fitMode_{L"fill"};
    float focalX_{0.5f};
    float focalY_{0.5f};
    bool innerFrame_{false};
    mutable std::wstring cachedPath_;
    mutable ID2D1RenderTarget* cachedTarget_{};
    mutable std::uint64_t cachedGeneration_{};
    mutable bool loadAttempted_{false};
    mutable Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap_;
};

} // namespace ws
