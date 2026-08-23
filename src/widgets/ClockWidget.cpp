#include "widgets/ClockWidget.h"

#include "layout/AuthoredContentLayout.h"
#include "rendering/WidgetRenderContext.h"
#include "rendering/ScopedRenderTransform.h"

#include <array>
#include <chrono>
#include <cwchar>
#include <d2d1helper.h>
#include <iterator>
#include <memory>
#include <windows.h>
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

HRESULT CreateFormat(
    IDWriteFactory& factory, float size, DWRITE_FONT_WEIGHT weight, IDWriteTextFormat** format) {
    HRESULT result = factory.CreateTextFormat(L"Segoe UI Variable", nullptr, weight,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", format);
    if (FAILED(result)) {
        result = factory.CreateTextFormat(L"Segoe UI", nullptr, weight,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", format);
    }
    return result;
}

} // namespace

HRESULT ClockWidget::EnsureTextFormats(IDWriteFactory& factory) const {
    if (timeFormat_ && dateTextFormat_) return S_OK;
    timeFormat_.Reset();
    dateTextFormat_.Reset();
    HRESULT result = CreateFormat(factory, 47.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, timeFormat_.GetAddressOf());
    if (FAILED(result)) return result;
    result = CreateFormat(factory, 15.0f, DWRITE_FONT_WEIGHT_NORMAL, dateTextFormat_.GetAddressOf());
    if (FAILED(result)) return result;
    result = timeFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    if (FAILED(result)) return result;
    result = timeFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (FAILED(result)) return result;
    result = timeFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    if (FAILED(result)) return result;
    result = dateTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    if (FAILED(result)) return result;
    result = dateTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (FAILED(result)) return result;
    return dateTextFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
}

void ClockWidget::Render(const WidgetRenderContext& context) const {
    if (FAILED(EnsureTextFormats(context.dwriteFactory))) return;
    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);

    std::array<wchar_t, 128> timeText{};
    const wchar_t* timePattern = use24Hour_
        ? (showSeconds_ ? L"HH:mm:ss" : L"HH:mm")
        : (showSeconds_ ? L"h:mm:ss tt" : L"h:mm tt");
    if (GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &localTime, timePattern,
            timeText.data(), static_cast<int>(timeText.size())) == 0) return;

    std::array<wchar_t, 192> dateText{};
    const wchar_t* datePattern = dateFormat_ == L"short" ? L"M/d/yyyy" :
        (dateFormat_ == L"compact" ? L"ddd, MMM d" : L"dddd,\nMMMM d\nyyyy");
    if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &localTime, datePattern,
            dateText.data(), static_cast<int>(dateText.size()), nullptr) == 0) return;

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> primaryBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> secondaryBrush;
    const D2D1_COLOR_F primary = context.lightAppearance
        ? D2D1::ColorF(0.06f, 0.065f, 0.075f, 0.96f)
        : D2D1::ColorF(0.98f, 0.98f, 0.97f, 0.98f);
    const D2D1_COLOR_F secondary = context.lightAppearance
        ? D2D1::ColorF(0.06f, 0.065f, 0.075f, 0.48f)
        : D2D1::ColorF(0.98f, 0.98f, 0.97f, 0.52f);
    if (FAILED(context.renderTarget.CreateSolidColorBrush(primary, primaryBrush.GetAddressOf())) ||
        FAILED(context.renderTarget.CreateSolidColorBrush(secondary, secondaryBrush.GetAddressOf()))) return;

    const AuthoredLayoutResult fit = AuthoredContentLayout::FitReference(
        SizeF{270.0f, 120.0f}, context.bounds, context.contentScale);
    const D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Scale(fit.scale, fit.scale) *
        D2D1::Matrix3x2F::Translation(fit.origin.x, fit.origin.y);
    const ScopedRenderTransform scopedTransform(context.renderTarget, transform);

    const D2D1_RECT_F timeBounds = showDivider_
        ? D2D1::RectF(0.0f, 0.0f, 176.0f, 120.0f)
        : D2D1::RectF(0.0f, 0.0f, 270.0f, 72.0f);
    const D2D1_RECT_F dateBounds = showDivider_
        ? D2D1::RectF(190.0f, 8.0f, 270.0f, 112.0f)
        : D2D1::RectF(0.0f, 70.0f, 270.0f, 120.0f);
    context.renderTarget.DrawTextW(timeText.data(), static_cast<UINT32>(wcslen(timeText.data())),
        timeFormat_.Get(), &timeBounds, primaryBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    context.renderTarget.DrawTextW(dateText.data(), static_cast<UINT32>(wcslen(dateText.data())),
        dateTextFormat_.Get(), &dateBounds, secondaryBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    if (showDivider_) {
        context.renderTarget.DrawLine(D2D1::Point2F(182.0f, 23.0f), D2D1::Point2F(182.0f, 97.0f),
            secondaryBrush.Get(), 1.0f);
    }
}

std::span<const WidgetSettingDefinition> ClockWidget::Settings() const noexcept {
    static const std::array definitions{
        WidgetSettingDefinition{L"use24Hour", L"24-hour time", WidgetSettingKind::Boolean},
        WidgetSettingDefinition{L"showSeconds", L"Show seconds", WidgetSettingKind::Boolean},
        WidgetSettingDefinition{L"showDivider", L"Show divider", WidgetSettingKind::Boolean},
        WidgetSettingDefinition{L"dateFormat", L"Date format", WidgetSettingKind::Choice,
            {L"long", L"short", L"compact"}},
    };
    return definitions;
}

WidgetState ClockWidget::SaveState() const {
    return WidgetState{
        {L"use24Hour", use24Hour_ ? L"true" : L"false"},
        {L"showSeconds", showSeconds_ ? L"true" : L"false"},
        {L"showDivider", showDivider_ ? L"true" : L"false"},
        {L"dateFormat", dateFormat_},
    };
}

void ClockWidget::RestoreState(const WidgetState& state) {
    use24Hour_ = ReadBool(state, L"use24Hour", use24Hour_);
    showSeconds_ = ReadBool(state, L"showSeconds", showSeconds_);
    showDivider_ = ReadBool(state, L"showDivider", showDivider_);
    const auto dateFormat = state.find(L"dateFormat");
    if (dateFormat != state.end() &&
        (dateFormat->second == L"long" || dateFormat->second == L"short" || dateFormat->second == L"compact")) {
        dateFormat_ = dateFormat->second;
    }
}

std::optional<std::chrono::system_clock::time_point> ClockWidget::NextUpdateTime() const noexcept {
    const auto now = std::chrono::system_clock::now();
    if (showSeconds_) return std::chrono::floor<std::chrono::seconds>(now) + std::chrono::seconds(1);
    return std::chrono::floor<std::chrono::minutes>(now) + std::chrono::minutes(1);
}

WidgetDescriptor ClockWidget::Descriptor() {
    return WidgetDescriptor{
        .typeId = "clock",
        .displayName = L"Clock",
        .description = L"Local time and date with configurable formatting.",
        .defaultGridSize = GridSize{4, 2},
        .minimumGridSize = GridSize{3, 2},
        .maximumGridSize = GridSize{6, 3},
        .capabilities = WidgetCapability::Configurable | WidgetCapability::Scalable,
        .factory = [] { return std::make_unique<ClockWidget>(); },
    };
}

} // namespace ws
