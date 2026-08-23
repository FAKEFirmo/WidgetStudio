#include "widgets/MusicWidget.h"

#include "layout/AuthoredContentLayout.h"
#include "rendering/ScopedRenderTransform.h"
#include "rendering/WidgetRenderContext.h"
#include "windows/MediaSessionService.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cwchar>
#include <limits>
#include <memory>
#include <utility>
#include <wincodec.h>
#include <wrl/client.h>

namespace ws {
namespace {

constexpr float infinity = std::numeric_limits<float>::infinity();
constexpr std::array kProfiles{
    AuthoredLayoutProfile{"portrait", 0.0f, 0.90f, {240.0f, 340.0f}},
    AuthoredLayoutProfile{"square", 0.90f, 1.45f, {300.0f, 300.0f}},
    AuthoredLayoutProfile{"landscape", 1.45f, 2.35f, {390.0f, 215.0f}},
    AuthoredLayoutProfile{"ultra-wide", 2.35f, infinity, {520.0f, 150.0f}},
};

struct MusicComposition {
    RectF artwork;
    RectF title;
    RectF artist;
    RectF status;
    RectF progress;
    RectF elapsed;
    RectF remaining;
    RectF previous;
    RectF playPause;
    RectF next;
};

MusicComposition Composition(std::size_t profile) noexcept {
    switch (profile) {
    case 0: return {{40, 0, 160, 160}, {16, 170, 208, 28}, {16, 198, 208, 20}, {16, 218, 208, 18},
                    {16, 246, 208, 5}, {16, 254, 70, 18}, {154, 254, 70, 18},
                    {50, 286, 38, 38}, {101, 282, 46, 46}, {160, 286, 38, 38}};
    case 1: return {{85, 0, 130, 130}, {20, 136, 260, 27}, {20, 162, 260, 19}, {20, 180, 260, 17},
                    {20, 205, 260, 5}, {20, 213, 65, 17}, {215, 213, 65, 17},
                    {72, 245, 34, 34}, {128, 239, 46, 46}, {194, 245, 34, 34}};
    case 2: return {{0, 0, 145, 145}, {164, 12, 216, 27}, {164, 42, 216, 20}, {164, 64, 216, 18},
                    {0, 158, 390, 5}, {0, 166, 70, 17}, {320, 166, 70, 17},
                    {135, 178, 34, 34}, {178, 172, 46, 46}, {229, 178, 34, 34}};
    default: return {{0, 0, 150, 150}, {170, 12, 230, 27}, {170, 41, 230, 20}, {170, 62, 230, 18},
                       {170, 86, 330, 5}, {170, 94, 70, 17}, {430, 94, 70, 17},
                       {367, 112, 30, 30}, {407, 106, 42, 42}, {459, 112, 30, 30}};
    }
}

HRESULT CreateFormat(IDWriteFactory& factory, float size, DWRITE_FONT_WEIGHT weight, IDWriteTextFormat** format) {
    HRESULT result = factory.CreateTextFormat(L"Segoe UI Variable", nullptr, weight,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", format);
    if (FAILED(result)) result = factory.CreateTextFormat(L"Segoe UI", nullptr, weight,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", format);
    return result;
}

D2D1_RECT_F D2DRect(RectF rect) noexcept {
    return D2D1::RectF(rect.Left(), rect.Top(), rect.Right(), rect.Bottom());
}

std::wstring DurationText(std::chrono::milliseconds duration, bool negative) {
    const auto totalSeconds = std::max<long long>(0, duration.count() / 1000);
    wchar_t buffer[32]{};
    _snwprintf_s(buffer, _countof(buffer), _TRUNCATE, negative ? L"-%lld:%02lld" : L"%lld:%02lld",
        totalSeconds / 60, totalSeconds % 60);
    return buffer;
}

void DrawTransportIcon(ID2D1RenderTarget& target, RectF bounds, std::string_view action,
    bool playing, ID2D1Brush& brush) {
    const float centerX = bounds.x + bounds.width * 0.5f;
    const float centerY = bounds.y + bounds.height * 0.5f;
    const float radius = std::min(bounds.width, bounds.height) * 0.5f;
    target.DrawEllipse(D2D1::Ellipse(D2D1::Point2F(centerX, centerY), radius - 1.0f, radius - 1.0f), &brush, 1.5f);
    const float half = radius * 0.34f;
    if (action == "play-pause") {
        if (playing) {
            target.DrawLine(D2D1::Point2F(centerX - half * 0.55f, centerY - half),
                D2D1::Point2F(centerX - half * 0.55f, centerY + half), &brush, 2.8f);
            target.DrawLine(D2D1::Point2F(centerX + half * 0.55f, centerY - half),
                D2D1::Point2F(centerX + half * 0.55f, centerY + half), &brush, 2.8f);
        } else {
            target.DrawLine(D2D1::Point2F(centerX - half * 0.65f, centerY - half),
                D2D1::Point2F(centerX + half, centerY), &brush, 2.5f);
            target.DrawLine(D2D1::Point2F(centerX + half, centerY),
                D2D1::Point2F(centerX - half * 0.65f, centerY + half), &brush, 2.5f);
            target.DrawLine(D2D1::Point2F(centerX - half * 0.65f, centerY + half),
                D2D1::Point2F(centerX - half * 0.65f, centerY - half), &brush, 2.5f);
        }
    } else {
        const float direction = action == "next" ? 1.0f : -1.0f;
        const float barX = centerX + direction * half;
        target.DrawLine(D2D1::Point2F(barX, centerY - half), D2D1::Point2F(barX, centerY + half), &brush, 2.5f);
        target.DrawLine(D2D1::Point2F(centerX - direction * half, centerY - half),
            D2D1::Point2F(centerX + direction * half * 0.65f, centerY), &brush, 2.5f);
        target.DrawLine(D2D1::Point2F(centerX + direction * half * 0.65f, centerY),
            D2D1::Point2F(centerX - direction * half, centerY + half), &brush, 2.5f);
    }
}

} // namespace

MusicWidget::MusicWidget(std::shared_ptr<MediaSessionService> mediaSession)
    : mediaSession_(std::move(mediaSession)) {}

HRESULT MusicWidget::EnsureTextFormats(IDWriteFactory& factory) const {
    if (titleFormat_ && metadataFormat_ && smallFormat_) return S_OK;
    titleFormat_.Reset(); metadataFormat_.Reset(); smallFormat_.Reset();
    HRESULT result = CreateFormat(factory, 18.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, titleFormat_.GetAddressOf());
    if (FAILED(result)) return result;
    result = CreateFormat(factory, 13.0f, DWRITE_FONT_WEIGHT_NORMAL, metadataFormat_.GetAddressOf());
    if (FAILED(result)) return result;
    result = CreateFormat(factory, 10.0f, DWRITE_FONT_WEIGHT_NORMAL, smallFormat_.GetAddressOf());
    if (FAILED(result)) return result;
    const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    for (IDWriteTextFormat* format : {titleFormat_.Get(), metadataFormat_.Get(), smallFormat_.Get()}) {
        result = format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        if (FAILED(result)) return result;
        result = format->SetTrimming(&trimming, nullptr);
        if (FAILED(result)) return result;
    }
    return S_OK;
}

HRESULT MusicWidget::EnsureArtwork(
    const WidgetRenderContext& context, const MediaSessionSnapshot& snapshot) const {
    if (cachedResourceGeneration_ != context.resourceGeneration ||
        cachedArtworkRevision_ != snapshot.artworkRevision) {
        artwork_.Reset();
        cachedResourceGeneration_ = context.resourceGeneration;
        cachedArtworkRevision_ = snapshot.artworkRevision;
    }
    if (artwork_) return S_OK;
    if (!snapshot.artwork || snapshot.artwork->empty() ||
        snapshot.artwork->size() > std::numeric_limits<DWORD>::max()) return S_FALSE;
    Microsoft::WRL::ComPtr<IWICStream> stream;
    HRESULT result = context.wicFactory.CreateStream(stream.GetAddressOf());
    if (FAILED(result)) return result;
    result = stream->InitializeFromMemory(const_cast<BYTE*>(snapshot.artwork->data()),
        static_cast<DWORD>(snapshot.artwork->size()));
    if (FAILED(result)) return result;
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    result = context.wicFactory.CreateDecoderFromStream(stream.Get(), nullptr,
        WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    if (FAILED(result)) return result;
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    result = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(result)) return result;
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    result = context.wicFactory.CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(result)) return result;
    result = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut);
    if (FAILED(result)) return result;
    return context.renderTarget.CreateBitmapFromWicBitmap(converter.Get(), nullptr, artwork_.GetAddressOf());
}

void MusicWidget::Render(const WidgetRenderContext& context) const {
    if (!mediaSession_ || FAILED(EnsureTextFormats(context.dwriteFactory))) return;
    const MediaSessionSnapshot snapshot = mediaSession_->Snapshot();
    const AuthoredLayoutResult fit = AuthoredContentLayout::Fit(
        kProfiles, context.bounds, currentProfile_, context.contentScale);
    currentProfile_ = fit.profileIndex;
    const MusicComposition composition = Composition(fit.profileIndex);
    const D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Scale(fit.scale, fit.scale) *
        D2D1::Matrix3x2F::Translation(fit.origin.x, fit.origin.y);
    const ScopedRenderTransform scopedTransform(context.renderTarget, transform);

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> primaryBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> mutedBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> trackBrush;
    const bool light = context.lightAppearance;
    if (FAILED(context.renderTarget.CreateSolidColorBrush(light
            ? D2D1::ColorF(0.06f, 0.065f, 0.075f, 0.95f) : D2D1::ColorF(0.98f, 0.98f, 0.97f, 0.97f),
            primaryBrush.GetAddressOf())) ||
        FAILED(context.renderTarget.CreateSolidColorBrush(light
            ? D2D1::ColorF(0.06f, 0.065f, 0.075f, 0.50f) : D2D1::ColorF(0.98f, 0.98f, 0.97f, 0.54f),
            mutedBrush.GetAddressOf())) ||
        FAILED(context.renderTarget.CreateSolidColorBrush(light
            ? D2D1::ColorF(0.06f, 0.065f, 0.075f, 0.18f) : D2D1::ColorF(0.98f, 0.98f, 0.97f, 0.20f),
            trackBrush.GetAddressOf()))) return;

    const D2D1_RECT_F artworkBounds = D2DRect(composition.artwork);
    if (snapshot.hasSession && EnsureArtwork(context, snapshot) == S_OK && artwork_) {
        const D2D1_SIZE_F size = artwork_->GetSize();
        const float edge = std::min(size.width, size.height);
        const D2D1_RECT_F source = D2D1::RectF((size.width - edge) * 0.5f, (size.height - edge) * 0.5f,
            (size.width + edge) * 0.5f, (size.height + edge) * 0.5f);
        context.renderTarget.DrawBitmap(artwork_.Get(), &artworkBounds, 1.0f,
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &source);
    } else {
        context.renderTarget.FillRectangle(&artworkBounds, trackBrush.Get());
    }

    const std::wstring title = snapshot.hasSession && !snapshot.title.empty() ? snapshot.title : L"Nothing playing";
    const std::wstring artist = snapshot.hasSession ? snapshot.artist : L"Start media in any Windows app";
    const std::wstring status = snapshot.hasSession
        ? snapshot.source + (snapshot.playing ? L" • Playing" : L" • Paused") : L"Windows media session";
    D2D1_RECT_F bounds = D2DRect(composition.title);
    context.renderTarget.DrawTextW(title.c_str(), static_cast<UINT32>(title.size()), titleFormat_.Get(),
        &bounds, primaryBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    bounds = D2DRect(composition.artist);
    context.renderTarget.DrawTextW(artist.c_str(), static_cast<UINT32>(artist.size()), metadataFormat_.Get(),
        &bounds, mutedBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    bounds = D2DRect(composition.status);
    context.renderTarget.DrawTextW(status.c_str(), static_cast<UINT32>(status.size()), smallFormat_.Get(),
        &bounds, mutedBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

    auto position = snapshot.position;
    if (snapshot.playing) position += std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - snapshot.capturedAt);
    const auto duration = std::max(snapshot.duration, std::chrono::milliseconds{});
    position = std::clamp(position, std::chrono::milliseconds{}, duration);
    const float fraction = duration.count() > 0
        ? static_cast<float>(position.count()) / static_cast<float>(duration.count()) : 0.0f;
    const D2D1_RECT_F progressTrack = D2DRect(composition.progress);
    const D2D1_ROUNDED_RECT track = D2D1::RoundedRect(progressTrack, 2.5f, 2.5f);
    context.renderTarget.FillRoundedRectangle(&track, trackBrush.Get());
    D2D1_RECT_F filled = progressTrack;
    filled.right = filled.left + composition.progress.width * std::clamp(fraction, 0.0f, 1.0f);
    const D2D1_ROUNDED_RECT progress = D2D1::RoundedRect(filled, 2.5f, 2.5f);
    context.renderTarget.FillRoundedRectangle(&progress, primaryBrush.Get());

    const std::wstring elapsed = DurationText(position, false);
    const std::wstring remaining = DurationText(duration - position, true);
    bounds = D2DRect(composition.elapsed);
    context.renderTarget.DrawTextW(elapsed.c_str(), static_cast<UINT32>(elapsed.size()), smallFormat_.Get(),
        &bounds, mutedBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    bounds = D2DRect(composition.remaining);
    context.renderTarget.DrawTextW(remaining.c_str(), static_cast<UINT32>(remaining.size()), smallFormat_.Get(),
        &bounds, mutedBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

    DrawTransportIcon(context.renderTarget, composition.previous, "previous", snapshot.playing,
        *(snapshot.canPrevious ? primaryBrush.Get() : mutedBrush.Get()));
    DrawTransportIcon(context.renderTarget, composition.playPause, "play-pause", snapshot.playing,
        *(snapshot.canTogglePlayPause ? primaryBrush.Get() : mutedBrush.Get()));
    DrawTransportIcon(context.renderTarget, composition.next, "next", snapshot.playing,
        *(snapshot.canNext ? primaryBrush.Get() : mutedBrush.Get()));
}

std::span<const WidgetSettingDefinition> MusicWidget::Settings() const noexcept { return {}; }
WidgetState MusicWidget::SaveState() const { return {}; }
void MusicWidget::RestoreState(const WidgetState&) {}

std::optional<std::chrono::system_clock::time_point> MusicWidget::NextUpdateTime() const noexcept {
    if (!mediaSession_ || !mediaSession_->Snapshot().playing) return std::nullopt;
    return std::chrono::system_clock::now() + std::chrono::milliseconds(500);
}

std::optional<std::string> MusicWidget::HitTestAction(const WidgetHitTestContext& context) const {
    if (!mediaSession_) return std::nullopt;
    const MediaSessionSnapshot snapshot = mediaSession_->Snapshot();
    if (!snapshot.hasSession) return std::nullopt;
    const AuthoredLayoutResult fit = AuthoredContentLayout::Fit(
        kProfiles, context.bounds, currentProfile_, context.contentScale);
    currentProfile_ = fit.profileIndex;
    if (fit.scale <= 0.0f) return std::nullopt;
    const PointF referencePoint{(context.point.x - fit.origin.x) / fit.scale,
        (context.point.y - fit.origin.y) / fit.scale};
    const MusicComposition composition = Composition(fit.profileIndex);
    if (snapshot.canPrevious && composition.previous.Contains(referencePoint)) return std::string("previous");
    if (snapshot.canTogglePlayPause && composition.playPause.Contains(referencePoint)) return std::string("play-pause");
    if (snapshot.canNext && composition.next.Contains(referencePoint)) return std::string("next");
    return std::nullopt;
}

bool MusicWidget::InvokeAction(std::string_view actionId) {
    if (!mediaSession_) return false;
    if (actionId == "previous") return mediaSession_->Previous();
    if (actionId == "play-pause") return mediaSession_->TogglePlayPause();
    if (actionId == "next") return mediaSession_->Next();
    return false;
}

WidgetDescriptor MusicWidget::Descriptor(std::shared_ptr<MediaSessionService> mediaSession) {
    return WidgetDescriptor{
        .typeId = "music",
        .displayName = L"Music",
        .description = L"Controls the active Windows media session.",
        .defaultGridSize = GridSize{7, 3},
        .minimumGridSize = GridSize{3, 3},
        .maximumGridSize = GridSize{10, 6},
        .capabilities = WidgetCapability::Interactive | WidgetCapability::Scalable,
        .factory = [mediaSession = std::move(mediaSession)] { return std::make_unique<MusicWidget>(mediaSession); },
    };
}

} // namespace ws
