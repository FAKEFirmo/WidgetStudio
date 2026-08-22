#pragma once

#include <cstdint>
#include <string>

namespace ws {

enum class WidgetType {
    Clock,
    Calendar,
    Music,
    Photo,
};

struct GridPlacement {
    int column{};
    int row{};
    int columnSpan{1};
    int rowSpan{1};
};

struct WidgetAppearance {
    float opacity{0.62f};
    float cornerRadius{22.0f};
    float contentScale{1.0f};
};

struct WidgetInstance {
    std::uint64_t id{};
    WidgetType type{WidgetType::Clock};
    GridPlacement grid{};
    WidgetAppearance appearance{};
    bool locked{false};
    bool selected{false};
    bool primarySelection{false};
};

inline std::wstring WidgetTypeName(WidgetType type) {
    switch (type) {
    case WidgetType::Clock: return L"Clock";
    case WidgetType::Calendar: return L"Calendar";
    case WidgetType::Music: return L"Music";
    case WidgetType::Photo: return L"Photo";
    }
    return L"Widget";
}

} // namespace ws
