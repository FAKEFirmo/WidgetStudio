#include "widgets/PhotoWidget.h"

#include "rendering/WidgetRenderContext.h"
#include "widgets/photo/PhotoLayout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <d2d1helper.h>
#include <iterator>
#include <memory>
#include <string_view>
#include <utility>
#include <wincodec.h>
#include <wrl/client.h>

namespace ws {
namespace {

bool ReadBool(const WidgetState& state, const wchar_t* key, bool fallback) {
    const auto found = state.find(key);
    if (found == state.end()) return fallback;
    if (found->second == L"true") return true;
    if (found->second == L"false") return false;
    return fallback;
}

float ReadNormalized(const WidgetState& state, const wchar_t* key, float fallback) {
    const auto found = state.find(key);
    if (found == state.end()) return fallback;
    wchar_t* end = nullptr;
    const float value = std::wcstof(found->second.c_str(), &end);
    if (end == found->second.c_str() || *end != L'\0' || !std::isfinite(value)) return fallback;
    return std::clamp(value, 0.0f, 1.0f);
}

std::wstring FloatText(float value) {
    wchar_t buffer[32]{};
    _snwprintf_s(buffer, _countof(buffer), _TRUNCATE, L"%.4g", value);
    return buffer;
}

} // namespace

ID2D1Bitmap* PhotoWidget::BitmapFor(const WidgetRenderContext& context) const {
    const std::filesystem::path resolvedPath = ResolvedAssetPath();
    const std::wstring path = resolvedPath.wstring();
    auto found = std::find_if(bitmapCache_.begin(), bitmapCache_.end(), [&context](const auto& entry) {
        return entry.target == &context.renderTarget;
    });
    if (found == bitmapCache_.end()) {
        // A widget normally renders to its desktop window and the Studio preview.
        // Keep those target-specific bitmaps independent because Direct2D resources
        // cannot be shared blindly across render-target resource domains.
        if (bitmapCache_.size() >= 4) bitmapCache_.erase(bitmapCache_.begin());
        bitmapCache_.push_back(BitmapCacheEntry{.target = &context.renderTarget});
        found = std::prev(bitmapCache_.end());
    }
    BitmapCacheEntry& cache = *found;
    if (cache.resourceGeneration != context.resourceGeneration || cache.path != path) {
        cache.resourceGeneration = context.resourceGeneration;
        cache.path = path;
        cache.loadAttempted = false;
        cache.bitmap.Reset();
    }
    if (cache.bitmap) return cache.bitmap.Get();
    if (cache.loadAttempted || assetPath_.empty() || resolvedPath.empty()) return nullptr;
    cache.loadAttempted = true;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT result = context.wicFactory.CreateDecoderFromFilename(
        resolvedPath.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    if (FAILED(result)) return nullptr;
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    result = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(result)) return nullptr;
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    result = context.wicFactory.CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(result)) return nullptr;
    result = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut);
    if (FAILED(result)) return nullptr;
    result = context.renderTarget.CreateBitmapFromWicBitmap(
        converter.Get(), nullptr, cache.bitmap.GetAddressOf());
    return SUCCEEDED(result) ? cache.bitmap.Get() : nullptr;
}

std::filesystem::path PhotoWidget::ResolvedAssetPath() const {
    constexpr std::wstring_view prefix = L"asset://";
    if (!assetPath_.starts_with(prefix)) return assetPath_;
    const std::filesystem::path filename(assetPath_.substr(prefix.size()));
    if (filename.empty() || filename.has_parent_path()) return {};
    return assetDirectory_ / filename;
}

void PhotoWidget::Render(const WidgetRenderContext& context) const {
    ID2D1Bitmap* bitmap = BitmapFor(context);
    if (!bitmap) {
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
        if (FAILED(context.renderTarget.CreateSolidColorBrush(context.lightAppearance
                ? D2D1::ColorF(0.08f, 0.08f, 0.09f, 0.52f)
                : D2D1::ColorF(0.98f, 0.98f, 0.97f, 0.56f), brush.GetAddressOf()))) return;
        const wchar_t message[] = L"Choose a local photo in Widget Studio";
        const D2D1_RECT_F bounds = D2D1::RectF(
            context.bounds.Left(), context.bounds.Top(), context.bounds.Right(), context.bounds.Bottom());
        context.renderTarget.DrawTextW(message, static_cast<UINT32>(std::size(message) - 1),
            &context.detailFormat, &bounds, brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        return;
    }

    const float contentScale = std::clamp(context.contentScale, 0.25f, 4.0f);
    RectF target{
        context.bounds.x + context.bounds.width * (1.0f - contentScale) * 0.5f,
        context.bounds.y + context.bounds.height * (1.0f - contentScale) * 0.5f,
        context.bounds.width * contentScale,
        context.bounds.height * contentScale,
    };
    if (innerFrame_) {
        const float inset = 5.0f * contentScale;
        target = RectF{target.x + inset, target.y + inset,
            std::max(0.0f, target.width - inset * 2.0f), std::max(0.0f, target.height - inset * 2.0f)};
    }
    if (target.width <= 0.0f || target.height <= 0.0f) return;

    const D2D1_SIZE_F image = bitmap->GetSize();
    if (image.width <= 0.0f || image.height <= 0.0f) return;
    const PhotoLayoutResult layout = PhotoLayout::Calculate(
        SizeF{image.width, image.height}, target,
        fitMode_ == L"fit" ? PhotoFitMode::Fit : PhotoFitMode::Fill, focalX_, focalY_);
    const D2D1_RECT_F destination = D2D1::RectF(layout.destination.Left(), layout.destination.Top(),
        layout.destination.Right(), layout.destination.Bottom());
    const D2D1_RECT_F source = D2D1::RectF(layout.source.Left(), layout.source.Top(),
        layout.source.Right(), layout.source.Bottom());

    context.renderTarget.DrawBitmap(bitmap, &destination, 1.0f,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &source);
    if (innerFrame_) {
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> frameBrush;
        if (SUCCEEDED(context.renderTarget.CreateSolidColorBrush(context.lightAppearance
                ? D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.18f)
                : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.22f), frameBrush.GetAddressOf()))) {
            context.renderTarget.DrawRectangle(&destination, frameBrush.Get(), 1.0f);
        }
    }
}

std::span<const WidgetSettingDefinition> PhotoWidget::Settings() const noexcept {
    static const std::array definitions{
        WidgetSettingDefinition{L"assetPath", L"Local image", WidgetSettingKind::File},
        WidgetSettingDefinition{L"fitMode", L"Image fit", WidgetSettingKind::Choice,
            {L"fill", L"fit"}, 0.0, 0.0, 0.0,
            {L"Fill (crop to card)", L"Fit (show entire image)"}},
        WidgetSettingDefinition{L"focalX", L"Horizontal focal point (0-1)", WidgetSettingKind::Number, {}, 0.0, 1.0, 0.01},
        WidgetSettingDefinition{L"focalY", L"Vertical focal point (0-1)", WidgetSettingKind::Number, {}, 0.0, 1.0, 0.01},
        WidgetSettingDefinition{L"innerFrame", L"Inner frame", WidgetSettingKind::Boolean},
    };
    return definitions;
}

WidgetState PhotoWidget::SaveState() const {
    return {{L"assetPath", assetPath_}, {L"fitMode", fitMode_},
            {L"focalX", FloatText(focalX_)}, {L"focalY", FloatText(focalY_)},
            {L"innerFrame", innerFrame_ ? L"true" : L"false"}};
}

void PhotoWidget::RestoreState(const WidgetState& state) {
    const auto path = state.find(L"assetPath");
    if (path != state.end() && path->second != assetPath_) {
        assetPath_ = path->second;
        bitmapCache_.clear();
    }
    const auto fit = state.find(L"fitMode");
    if (fit != state.end() && (fit->second == L"fill" || fit->second == L"fit")) fitMode_ = fit->second;
    focalX_ = ReadNormalized(state, L"focalX", focalX_);
    focalY_ = ReadNormalized(state, L"focalY", focalY_);
    innerFrame_ = ReadBool(state, L"innerFrame", innerFrame_);
}

WidgetDescriptor PhotoWidget::Descriptor(std::filesystem::path assetDirectory) {
    return WidgetDescriptor{
        .typeId = "photo",
        .displayName = L"Photo",
        .description = L"Displays a locally imported image using proportional fill or fit.",
        .defaultGridSize = GridSize{4, 3},
        .minimumGridSize = GridSize{2, 2},
        .maximumGridSize = GridSize{8, 6},
        .capabilities = WidgetCapability::Configurable | WidgetCapability::Scalable |
            WidgetCapability::Resizable | WidgetCapability::Duplicatable |
            WidgetCapability::PassiveClickThrough,
        .factory = [assetDirectory = std::move(assetDirectory)] {
            return std::make_unique<PhotoWidget>(assetDirectory);
        },
    };
}

} // namespace ws
