#pragma once

#include "widgets/IWidget.h"
#include "widgets/WidgetDescriptor.h"

#include <d2d1.h>
#include <cstdint>
#include <filesystem>
#include <utility>
#include <vector>
#include <wrl/client.h>

namespace ws {

class PhotoWidget final : public IWidget {
public:
    explicit PhotoWidget(std::filesystem::path assetDirectory)
        : assetDirectory_(std::move(assetDirectory)) {}
    void Render(const WidgetRenderContext& context) const override;
    [[nodiscard]] std::span<const WidgetSettingDefinition> Settings() const noexcept override;
    [[nodiscard]] WidgetState SaveState() const override;
    void RestoreState(const WidgetState& state) override;

    [[nodiscard]] static WidgetDescriptor Descriptor(std::filesystem::path assetDirectory);

private:
    [[nodiscard]] ID2D1Bitmap* BitmapFor(const WidgetRenderContext& context) const;
    [[nodiscard]] std::filesystem::path ResolvedAssetPath() const;

    struct BitmapCacheEntry {
        ID2D1RenderTarget* target{};
        std::uint64_t resourceGeneration{};
        std::wstring path;
        bool loadAttempted{false};
        Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    };

    std::filesystem::path assetDirectory_;
    std::wstring assetPath_;
    std::wstring fitMode_{L"fill"};
    float focalX_{0.5f};
    float focalY_{0.5f};
    bool innerFrame_{false};
    mutable std::vector<BitmapCacheEntry> bitmapCache_;
};

} // namespace ws
