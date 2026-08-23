#include "widgets/calendar/CalendarModel.h"

#include <algorithm>

namespace ws {

bool CalendarModel::IsLeapYear(int year) noexcept {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int CalendarModel::DaysInMonth(int year, int month) noexcept {
    constexpr std::array days{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int normalized = std::clamp(month, 1, 12);
    if (normalized == 2 && IsLeapYear(year)) return 29;
    return days[static_cast<std::size_t>(normalized - 1)];
}

int CalendarModel::DayOfWeek(CivilDate date) noexcept {
    // Sakamoto's algorithm: 0 = Sunday, 6 = Saturday.
    constexpr std::array offsets{0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int year = date.year;
    if (date.month < 3) --year;
    const int value = year + year / 4 - year / 100 + year / 400 +
        offsets[static_cast<std::size_t>(std::clamp(date.month, 1, 12) - 1)] + date.day;
    return ((value % 7) + 7) % 7;
}

CivilDate CalendarModel::AddDays(CivilDate date, int days) noexcept {
    while (days < 0) {
        --date.day;
        ++days;
        if (date.day < 1) {
            if (--date.month < 1) { date.month = 12; --date.year; }
            date.day = DaysInMonth(date.year, date.month);
        }
    }
    while (days > 0) {
        ++date.day;
        --days;
        if (date.day > DaysInMonth(date.year, date.month)) {
            date.day = 1;
            if (++date.month > 12) { date.month = 1; ++date.year; }
        }
    }
    return date;
}

std::array<CalendarCell, 42> CalendarModel::Build(
    int year, int month, bool mondayFirst, CivilDate today) noexcept {
    std::array<CalendarCell, 42> cells{};
    const CivilDate first{year, std::clamp(month, 1, 12), 1};
    const int firstWeekday = DayOfWeek(first);
    const int leading = mondayFirst ? (firstWeekday + 6) % 7 : firstWeekday;
    const CivilDate gridStart = AddDays(first, -leading);
    for (std::size_t index = 0; index < cells.size(); ++index) {
        const CivilDate date = AddDays(gridStart, static_cast<int>(index));
        const int weekday = DayOfWeek(date);
        cells[index] = CalendarCell{
            .date = date,
            .inCurrentMonth = date.year == first.year && date.month == first.month,
            .today = date.year == today.year && date.month == today.month && date.day == today.day,
            .weekend = weekday == 0 || weekday == 6,
        };
    }
    return cells;
}

} // namespace ws
