#pragma once

#include <array>

namespace ws {

struct CivilDate {
    int year{};
    int month{};
    int day{};
};

struct CalendarCell {
    CivilDate date{};
    bool inCurrentMonth{false};
    bool today{false};
    bool weekend{false};
};

class CalendarModel {
public:
    [[nodiscard]] static bool IsLeapYear(int year) noexcept;
    [[nodiscard]] static int DaysInMonth(int year, int month) noexcept;
    [[nodiscard]] static int DayOfWeek(CivilDate date) noexcept;
    [[nodiscard]] static CivilDate AddDays(CivilDate date, int days) noexcept;
    [[nodiscard]] static std::array<CalendarCell, 42> Build(
        int year, int month, bool mondayFirst, CivilDate today) noexcept;
};

} // namespace ws
