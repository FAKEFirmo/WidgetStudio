#pragma once

#include "common/Geometry.h"
#include "layout/GridLayout.h"
#include "scene/WidgetInstance.h"
#include "persistence/WidgetPersistence.h"
#include "widgets/WidgetDescriptor.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ws {

class WidgetRegistry;

class WidgetScene {
public:
    explicit WidgetScene(const WidgetRegistry& registry);

    [[nodiscard]] const std::vector<WidgetInstance>& Widgets() const noexcept { return widgets_; }
    [[nodiscard]] std::vector<WidgetInstance>& Widgets() noexcept { return widgets_; }

    [[nodiscard]] WidgetInstance* Find(std::string_view instanceId) noexcept;
    [[nodiscard]] const WidgetInstance* Find(std::string_view instanceId) const noexcept;

    [[nodiscard]] std::optional<std::string> HitTest(
        PointF point,
        const GridLayout& layout,
        const GridMetrics& metrics) const noexcept;

    void Select(std::string_view instanceId, bool additive) noexcept;
    void ClearSelection() noexcept;

    [[nodiscard]] std::optional<std::string> PrimarySelection() const;
    [[nodiscard]] std::size_t SelectionCount() const noexcept;

    WidgetInstance* CreateWidget(std::string_view typeId, std::wstring_view monitorId = L"primary");
    bool RemoveWidget(std::string_view instanceId) noexcept;
    std::size_t RemoveSelectedWidgets() noexcept;
    WidgetInstance* DuplicateWidget(std::string_view instanceId);
    bool SetWidgetLocked(std::string_view instanceId, bool locked) noexcept;
    [[nodiscard]] WidgetSceneSnapshot Snapshot() const;
    WidgetInstance* RestoreWidget(const WidgetPersistenceRecord& record, bool select = false);

    void SetGridDimensions(int columns, int rows) noexcept {
        gridColumns_ = std::max(1, columns);
        gridRows_ = std::max(1, rows);
    }

private:
    [[nodiscard]] GridPlacement FindInitialPlacement(
        GridSize size, std::wstring_view monitorId) const noexcept;
    const WidgetRegistry& registry_;
    std::vector<WidgetInstance> widgets_;
    int gridColumns_{12};
    int gridRows_{7};
};

} // namespace ws
