#pragma once

#include "widgets/IWidget.h"
#include "widgets/WidgetDescriptor.h"

#include <d2d1.h>
#include <dwrite.h>
#include <memory>
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
    HRESULT EnsureArtwork(const WidgetRenderContext& context, const MediaSessionSnapshot& snapshot) const;

    std::shared_ptr<MediaSessionService> mediaSession_;
    mutable std::optional<std::size_t> currentProfile_;
    mutable std::uint64_t cachedArtworkRevision_{};
    mutable std::uint64_t cachedResourceGeneration_{};
    mutable Microsoft::WRL::ComPtr<ID2D1Bitmap> artwork_;
    mutable Microsoft::WRL::ComPtr<IDWriteTextFormat> titleFormat_;
    mutable Microsoft::WRL::ComPtr<IDWriteTextFormat> metadataFormat_;
    mutable Microsoft::WRL::ComPtr<IDWriteTextFormat> smallFormat_;
};

} // namespace ws
