#pragma once

#include "widgets/IWidget.h"
#include "widgets/WidgetDescriptor.h"

#include <d2d1.h>
#include <dwrite.h>
#include <cstdint>
#include <memory>
#include <vector>
#include <wrl/client.h>

namespace ws {

class MediaSessionService;
struct MediaSessionSnapshot;

class MusicWidget final : public IWidget {
public:
    explicit MusicWidget(std::shared_ptr<MediaSessionService> mediaSession);

    void Render(const WidgetRenderContext& context) const override;
    [[nodiscard]] std::span<const WidgetSettingDefinition> Settings() const noexcept override;
    [[nodiscard]] WidgetState SaveState() const override;
    void RestoreState(const WidgetState& state) override;
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> NextUpdateTime() const noexcept override;
    [[nodiscard]] std::optional<std::string> HitTestAction(
        const WidgetHitTestContext& context) const override;
    bool InvokeAction(std::string_view actionId) override;

    [[nodiscard]] static WidgetDescriptor Descriptor(std::shared_ptr<MediaSessionService> mediaSession);

private:
    HRESULT EnsureTextFormats(IDWriteFactory& factory) const;
    [[nodiscard]] ID2D1Bitmap* ArtworkFor(
        const WidgetRenderContext& context, const MediaSessionSnapshot& snapshot) const;

    struct ArtworkCacheEntry {
        ID2D1RenderTarget* target{};
        std::uint64_t resourceGeneration{};
        std::uint64_t artworkRevision{};
        bool loadAttempted{false};
        Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    };

    std::shared_ptr<MediaSessionService> mediaSession_;
    mutable std::optional<std::size_t> currentProfile_;
    mutable std::vector<ArtworkCacheEntry> artworkCache_;
    mutable Microsoft::WRL::ComPtr<IDWriteTextFormat> titleFormat_;
    mutable Microsoft::WRL::ComPtr<IDWriteTextFormat> metadataFormat_;
    mutable Microsoft::WRL::ComPtr<IDWriteTextFormat> smallFormat_;
};

} // namespace ws
