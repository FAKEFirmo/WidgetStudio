#pragma once

#include "common/Geometry.h"
#include "layout/GridLayout.h"
#include "scene/WidgetInstance.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace ws {

class WidgetScene {
public:
    WidgetScene();

    [[nodiscard]] const std::vector<WidgetInstance>& Widgets() const noexcept { return widgets_; }
    [[nodiscard]] std::vector<WidgetInstance>& Widgets() noexcept { return widgets_; }

    [[nodiscard]] WidgetInstance* Find(std::uint64_t id) noexcept;
    [[nodiscard]] const WidgetInstance* Find(std::uint64_t id) const noexcept;

    [[nodiscard]] std::optional<std::uint64_t> HitTest(
        PointF point,
        const GridLayout& layout,
        const GridMetrics& metrics) const noexcept;

    void Select(std::uint64_t id, bool additive) noexcept;
    void ClearSelection() noexcept;

    [[nodiscard]] std::optional<std::uint64_t> PrimarySelection() const noexcept;
    [[nodiscard]] std::size_t SelectionCount() const noexcept;

private:
    std::vector<WidgetInstance> widgets_;
};

} // namespace ws
