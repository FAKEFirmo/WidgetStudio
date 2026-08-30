#include "widgets/CalendarWidget.h"

#include "layout/AuthoredContentLayout.h"
#include "rendering/ScopedRenderTransform.h"
#include "rendering/WidgetRenderContext.h"
#include "widgets/calendar/CalendarModel.h"

#include <array>
#include <chrono>
#include <ctime>
#include <cwchar>
#include <d2d1helper.h>
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

HRESULT CreateFormat(IDWriteFactory& factory, float size, DWRITE_FONT_WEIGHT weight, IDWriteTextFormat** format) {
    HRESULT result = factory.CreateTextFormat(L"Segoe UI Variable", nullptr, weight,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", format);
    if (FAILED(result)) {
        result = factory.CreateTextFormat(L"Segoe UI", nullptr, weight,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", format);
    }
    return result;
}

std::chrono::system_clock::time_point FileTimeToSystemClock(FILETIME fileTime) noexcept {
    ULARGE_INTEGER ticks{};
    ticks.LowPart = fileTime.dwLowDateTime;
    ticks.HighPart = fileTime.dwHighDateTime;
    constexpr unsigned long long windowsToUnixEpoch = 116444736000000000ULL;
    if (ticks.QuadPart <= windowsToUnixEpoch) return std::chrono::system_clock::now() + std::chrono::hours(1);
    const unsigned long long unixTicks = ticks.QuadPart - windowsToUnixEpoch;
    const auto seconds = static_cast<std::time_t>(unixTicks / 10000000ULL);
    const auto remainder = std::chrono::nanoseconds((unixTicks % 10000000ULL) * 100ULL);
    return std::chrono::system_clock::from_time_t(seconds) +
        std::chrono::duration_cast<std::chrono::system_clock::duration>(remainder);
}

} // namespace

HRESULT CalendarWidget::EnsureTextFormats(IDWriteFactory& factory) const {
    if (monthFormat_ && yearFormat_ && weekdayFormat_ && dayFormat_) return S_OK;
    monthFormat_.Reset(); yearFormat_.Reset(); weekdayFormat_.Reset(); dayFormat_.Reset();
    HRESULT result = CreateFormat(factory, 25.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, monthFormat_.GetAddressOf());
    if (FAILED(result)) return result;
    result = CreateFormat(factory, 18.0f, DWRITE_FONT_WEIGHT_NORMAL, yearFormat_.GetAddressOf());
    if (FAILED(result)) return result;
    result = CreateFormat(factory, 11.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, weekdayFormat_.GetAddressOf());
    if (FAILED(result)) return result;
    result = CreateFormat(factory, 14.0f, DWRITE_FONT_WEIGHT_NORMAL, dayFormat_.GetAddressOf());
    if (FAILED(result)) return result;
    for (IDWriteTextFormat* format : {monthFormat_.Get(), yearFormat_.Get(), weekdayFormat_.Get(), dayFormat_.Get()}) {
        result = format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        if (FAILED(result)) return result;
        result = format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (FAILED(result)) return result;
    }
    result = yearFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    if (FAILED(result)) return result;
    result = weekdayFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    if (FAILED(result)) return result;
    return dayFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
}

void CalendarWidget::Render(const WidgetRenderContext& context) const {
    if (FAILED(EnsureTextFormats(context.dwriteFactory))) return;
    SYSTEMTIME now{};
    GetLocalTime(&now);
    const CivilDate today{now.wYear, now.wMonth, now.wDay};
    const auto cells = CalendarModel::Build(now.wYear, now.wMonth, mondayFirst_, today);

    std::array<wchar_t, 64> monthText{};
    std::array<wchar_t, 16> yearText{};
    SYSTEMTIME firstOfMonth = now;
    firstOfMonth.wDay = 1;
    if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &firstOfMonth, L"MMMM",
            monthText.data(), static_cast<int>(monthText.size()), nullptr) == 0 ||
        GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &firstOfMonth, L"yyyy",
            yearText.data(), static_cast<int>(yearText.size()), nullptr) == 0) return;

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> primaryBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> mutedBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> dimBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> todayBrush;
    const bool light = context.lightAppearance;
    if (FAILED(context.renderTarget.CreateSolidColorBrush(light
            ? D2D1::ColorF(0.06f, 0.065f, 0.075f, 0.94f) : D2D1::ColorF(0.98f, 0.98f, 0.97f, 0.96f),
            primaryBrush.GetAddressOf())) ||
        FAILED(context.renderTarget.CreateSolidColorBrush(light
            ? D2D1::ColorF(0.06f, 0.065f, 0.075f, 0.55f) : D2D1::ColorF(0.98f, 0.98f, 0.97f, 0.58f),
            mutedBrush.GetAddressOf())) ||
        FAILED(context.renderTarget.CreateSolidColorBrush(light
            ? D2D1::ColorF(0.06f, 0.065f, 0.075f, 0.28f) : D2D1::ColorF(0.98f, 0.98f, 0.97f, 0.30f),
            dimBrush.GetAddressOf())) ||
        FAILED(context.renderTarget.CreateSolidColorBrush(light
            ? D2D1::ColorF(0.12f, 0.35f, 0.92f, 0.92f) : D2D1::ColorF(0.38f, 0.60f, 1.0f, 0.94f),
            todayBrush.GetAddressOf()))) return;

    const AuthoredLayoutResult fit = AuthoredContentLayout::FitReference(
        SizeF{270.0f, 254.0f}, context.bounds, context.contentScale);
    const D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Scale(fit.scale, fit.scale) *
        D2D1::Matrix3x2F::Translation(fit.origin.x, fit.origin.y);
    const ScopedRenderTransform scopedTransform(context.renderTarget, transform);

    const D2D1_RECT_F monthBounds = D2D1::RectF(4.0f, 0.0f, 190.0f, 40.0f);
    const D2D1_RECT_F yearBounds = D2D1::RectF(185.0f, 0.0f, 266.0f, 40.0f);
    context.renderTarget.DrawTextW(monthText.data(), static_cast<UINT32>(wcslen(monthText.data())),
        monthFormat_.Get(), &monthBounds, primaryBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    context.renderTarget.DrawTextW(yearText.data(), static_cast<UINT32>(wcslen(yearText.data())),
        yearFormat_.Get(), &yearBounds, mutedBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

    constexpr float columnWidth = 270.0f / 7.0f;
    constexpr float gridTop = 75.0f;
    constexpr float rowHeight = (254.0f - gridTop) / 6.0f;
    const CivilDate weekdayOrigin{2024, 1, 7}; // Sunday.
    for (int column = 0; column < 7; ++column) {
        const int sundayBased = mondayFirst_ ? (column + 1) % 7 : column;
        const CivilDate weekdayDate = CalendarModel::AddDays(weekdayOrigin, sundayBased);
        SYSTEMTIME weekdayTime{};
        weekdayTime.wYear = static_cast<WORD>(weekdayDate.year);
        weekdayTime.wMonth = static_cast<WORD>(weekdayDate.month);
        weekdayTime.wDay = static_cast<WORD>(weekdayDate.day);
        std::array<wchar_t, 16> weekdayText{};
        if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &weekdayTime, L"ddd",
                weekdayText.data(), static_cast<int>(weekdayText.size()), nullptr) == 0) continue;
        const D2D1_RECT_F bounds = D2D1::RectF(column * columnWidth, 43.0f,
            (column + 1) * columnWidth, 69.0f);
        context.renderTarget.DrawTextW(weekdayText.data(), static_cast<UINT32>(wcslen(weekdayText.data())),
            weekdayFormat_.Get(), &bounds, mutedBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    for (std::size_t index = 0; index < cells.size(); ++index) {
        const int column = static_cast<int>(index % 7);
        const int row = static_cast<int>(index / 7);
        const float left = column * columnWidth;
        const float top = gridTop + row * rowHeight;
        const D2D1_RECT_F bounds = D2D1::RectF(left, top, left + columnWidth, top + rowHeight);
        wchar_t dayText[4]{};
        _snwprintf_s(dayText, _countof(dayText), _TRUNCATE, L"%d", cells[index].date.day);
        ID2D1SolidColorBrush* textBrush = primaryBrush.Get();
        if (!cells[index].inCurrentMonth || (dimWeekends_ && cells[index].weekend)) textBrush = dimBrush.Get();
        if (cells[index].today) {
            const float size = 25.0f;
            const D2D1_RECT_F highlightBounds = D2D1::RectF(
                left + (columnWidth - size) * 0.5f, top + (rowHeight - size) * 0.5f,
                left + (columnWidth + size) * 0.5f, top + (rowHeight + size) * 0.5f);
            const D2D1_ROUNDED_RECT highlight = D2D1::RoundedRect(highlightBounds, 8.0f, 8.0f);
            context.renderTarget.FillRoundedRectangle(&highlight, todayBrush.Get());
            textBrush = primaryBrush.Get();
        }
        context.renderTarget.DrawTextW(dayText, static_cast<UINT32>(wcslen(dayText)), dayFormat_.Get(),
            &bounds, textBrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
}

std::span<const WidgetSettingDefinition> CalendarWidget::Settings() const noexcept {
    static const std::array definitions{
        WidgetSettingDefinition{L"mondayFirst", L"Week starts Monday", WidgetSettingKind::Boolean},
        WidgetSettingDefinition{L"dimWeekends", L"Dim weekends", WidgetSettingKind::Boolean},
    };
    return definitions;
}

WidgetState CalendarWidget::SaveState() const {
    return {{L"mondayFirst", mondayFirst_ ? L"true" : L"false"},
            {L"dimWeekends", dimWeekends_ ? L"true" : L"false"}};
}

void CalendarWidget::RestoreState(const WidgetState& state) {
    mondayFirst_ = ReadBool(state, L"mondayFirst", mondayFirst_);
    dimWeekends_ = ReadBool(state, L"dimWeekends", dimWeekends_);
}

std::optional<std::chrono::system_clock::time_point> CalendarWidget::NextUpdateTime() const noexcept {
    SYSTEMTIME local{};
    GetLocalTime(&local);
    CivilDate nextDate = CalendarModel::AddDays(
        CivilDate{local.wYear, local.wMonth, local.wDay}, 1);
    SYSTEMTIME nextLocal{};
    nextLocal.wYear = static_cast<WORD>(nextDate.year);
    nextLocal.wMonth = static_cast<WORD>(nextDate.month);
    nextLocal.wDay = static_cast<WORD>(nextDate.day);
    SYSTEMTIME nextUtc{};
    FILETIME nextFileTime{};
    if (!TzSpecificLocalTimeToSystemTimeEx(nullptr, &nextLocal, &nextUtc) ||
        !SystemTimeToFileTime(&nextUtc, &nextFileTime)) {
        return std::chrono::system_clock::now() + std::chrono::hours(1);
    }
    return FileTimeToSystemClock(nextFileTime);
}

WidgetDescriptor CalendarWidget::Descriptor() {
    return WidgetDescriptor{
        .typeId = "calendar",
        .displayName = L"Calendar",
        .description = L"Local monthly calendar with configurable week start.",
        .defaultGridSize = GridSize{4, 4},
        .minimumGridSize = GridSize{3, 3},
        .maximumGridSize = GridSize{6, 6},
        .capabilities = WidgetCapability::Configurable | WidgetCapability::Scalable,
        .factory = [] { return std::make_unique<CalendarWidget>(); },
    };
}

} // namespace ws
