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

HRESULT PhotoWidget::EnsureBitmap(const WidgetRenderContext& context) const {
    if (cachedTarget_ != &context.renderTarget || cachedGeneration_ != context.resourceGeneration ||
        cachedPath_ != assetPath_) {
        bitmap_.Reset();
        cachedTarget_ = &context.renderTarget;
        cachedGeneration_ = context.resourceGeneration;
        cachedPath_ = assetPath_;
        loadAttempted_ = false;
    }
    if (bitmap_) return S_OK;
    if (loadAttempted_ || assetPath_.empty()) return S_FALSE;
    loadAttempted_ = true;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT result = context.wicFactory.CreateDecoderFromFilename(assetPath_.c_str(), nullptr, GENERIC_READ,
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
    return context.renderTarget.CreateBitmapFromWicBitmap(converter.Get(), nullptr, bitmap_.GetAddressOf());
}

void PhotoWidget::Render(const WidgetRenderContext& context) const {
    if (EnsureBitmap(context) != S_OK || !bitmap_) {
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

    RectF target = context.bounds;
    if (innerFrame_) {
        constexpr float inset = 5.0f;
        target = RectF{target.x + inset, target.y + inset,
            std::max(0.0f, target.width - inset * 2.0f), std::max(0.0f, target.height - inset * 2.0f)};
    }
    if (target.width <= 0.0f || target.height <= 0.0f) return;

    const D2D1_SIZE_F image = bitmap_->GetSize();
    if (image.width <= 0.0f || image.height <= 0.0f) return;
    const PhotoLayoutResult layout = PhotoLayout::Calculate(
        SizeF{image.width, image.height}, target,
        fitMode_ == L"fit" ? PhotoFitMode::Fit : PhotoFitMode::Fill, focalX_, focalY_);
    const D2D1_RECT_F destination = D2D1::RectF(layout.destination.Left(), layout.destination.Top(),
        layout.destination.Right(), layout.destination.Bottom());
    const D2D1_RECT_F source = D2D1::RectF(layout.source.Left(), layout.source.Top(),
        layout.source.Right(), layout.source.Bottom());

    context.renderTarget.DrawBitmap(bitmap_.Get(), &destination, 1.0f,
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
        WidgetSettingDefinition{L"fitMode", L"Image fit", WidgetSettingKind::Choice, {L"fill", L"fit"}},
        WidgetSettingDefinition{L"focalX", L"Horizontal focal point", WidgetSettingKind::Number, {}, 0.0, 1.0, 0.01},
        WidgetSettingDefinition{L"focalY", L"Vertical focal point", WidgetSettingKind::Number, {}, 0.0, 1.0, 0.01},
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
        bitmap_.Reset();
        loadAttempted_ = false;
    }
    const auto fit = state.find(L"fitMode");
    if (fit != state.end() && (fit->second == L"fill" || fit->second == L"fit")) fitMode_ = fit->second;
    focalX_ = ReadNormalized(state, L"focalX", focalX_);
    focalY_ = ReadNormalized(state, L"focalY", focalY_);
    innerFrame_ = ReadBool(state, L"innerFrame", innerFrame_);
}

WidgetDescriptor PhotoWidget::Descriptor() {
    return WidgetDescriptor{
        .typeId = "photo",
        .displayName = L"Photo",
        .description = L"Displays a locally imported image using proportional fill or fit.",
        .defaultGridSize = GridSize{4, 3},
        .minimumGridSize = GridSize{2, 2},
        .maximumGridSize = GridSize{8, 6},
        .capabilities = WidgetCapability::Configurable | WidgetCapability::Scalable,
        .factory = [] { return std::make_unique<PhotoWidget>(); },
    };
}

} // namespace ws
