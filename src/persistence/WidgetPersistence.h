#pragma once

#include "scene/WidgetInstance.h"

#include <string>
#include <initializer_list>
#include <vector>
#include <utility>

namespace ws {

// This encoding-neutral record keeps scene/domain code independent from the
// versioned JSON file format implemented by SceneJsonCodec.
struct WidgetPersistenceRecord {
    std::string instanceId;
    std::string typeId;
    std::wstring monitorId;
    LayoutMode layoutMode{LayoutMode::Grid};
    GridPlacement grid{};
    FreePlacement free{};
    bool locked{false};
    float contentScale{1.0f};
    WidgetAppearance appearance{};
    bool useGeneralAppearance{false};
    WidgetState widgetState{};
};

struct WidgetSceneSnapshot {
    WidgetAppearance generalAppearance{};
    std::vector<WidgetPersistenceRecord> widgets;

    WidgetSceneSnapshot() = default;
    WidgetSceneSnapshot(std::initializer_list<WidgetPersistenceRecord> records) : widgets(records) {}

    [[nodiscard]] std::size_t size() const noexcept { return widgets.size(); }
    [[nodiscard]] bool empty() const noexcept { return widgets.empty(); }
    void reserve(std::size_t count) { widgets.reserve(count); }
    void push_back(WidgetPersistenceRecord record) { widgets.push_back(std::move(record)); }
    [[nodiscard]] WidgetPersistenceRecord& front() { return widgets.front(); }
    [[nodiscard]] const WidgetPersistenceRecord& front() const { return widgets.front(); }
    [[nodiscard]] WidgetPersistenceRecord& back() { return widgets.back(); }
    [[nodiscard]] const WidgetPersistenceRecord& back() const { return widgets.back(); }
    [[nodiscard]] WidgetPersistenceRecord& operator[](std::size_t index) { return widgets[index]; }
    [[nodiscard]] const WidgetPersistenceRecord& operator[](std::size_t index) const { return widgets[index]; }
    [[nodiscard]] auto begin() noexcept { return widgets.begin(); }
    [[nodiscard]] auto end() noexcept { return widgets.end(); }
    [[nodiscard]] auto begin() const noexcept { return widgets.begin(); }
    [[nodiscard]] auto end() const noexcept { return widgets.end(); }
};

} // namespace ws
