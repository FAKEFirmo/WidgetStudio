#include "scene/WidgetScene.h"

#include <algorithm>

namespace ws {

WidgetScene::WidgetScene() {
    widgets_.push_back(WidgetInstance{
        .id = 1,
        .type = WidgetType::Clock,
        .grid = GridPlacement{0, 0, 4, 2},
    });
    widgets_.push_back(WidgetInstance{
        .id = 2,
        .type = WidgetType::Calendar,
        .grid = GridPlacement{0, 2, 4, 4},
    });
    widgets_.push_back(WidgetInstance{
        .id = 3,
        .type = WidgetType::Music,
        .grid = GridPlacement{4, 0, 7, 3},
    });
    widgets_.push_back(WidgetInstance{
        .id = 4,
        .type = WidgetType::Photo,
        .grid = GridPlacement{8, 3, 4, 3},
    });

    Select(3, false);
}

WidgetInstance* WidgetScene::Find(std::uint64_t id) noexcept {
    const auto it = std::find_if(widgets_.begin(), widgets_.end(), [id](const WidgetInstance& widget) {
        return widget.id == id;
    });
    return it == widgets_.end() ? nullptr : &*it;
}

const WidgetInstance* WidgetScene::Find(std::uint64_t id) const noexcept {
    const auto it = std::find_if(widgets_.begin(), widgets_.end(), [id](const WidgetInstance& widget) {
        return widget.id == id;
    });
    return it == widgets_.end() ? nullptr : &*it;
}

std::optional<std::uint64_t> WidgetScene::HitTest(
    PointF point,
    const GridLayout& layout,
    const GridMetrics& metrics) const noexcept {

    // Reverse iteration treats later widgets as visually higher in the scene.
    for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it) {
        if (layout.RectFor(it->grid, metrics).Contains(point)) {
            return it->id;
        }
    }
    return std::nullopt;
}

void WidgetScene::Select(std::uint64_t id, bool additive) noexcept {
    if (!additive) {
        for (auto& widget : widgets_) {
            widget.selected = false;
            widget.primarySelection = false;
        }
    }

    WidgetInstance* target = Find(id);
    if (!target) {
        return;
    }

    if (additive && target->selected) {
        target->selected = false;
        target->primarySelection = false;

        // Promote the last remaining selected widget, if any.
        for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it) {
            if (it->selected) {
                it->primarySelection = true;
                break;
            }
        }
        return;
    }

    for (auto& widget : widgets_) {
        if (widget.selected) {
            widget.primarySelection = false;
        }
    }

    target->selected = true;
    target->primarySelection = true;
}

void WidgetScene::ClearSelection() noexcept {
    for (auto& widget : widgets_) {
        widget.selected = false;
        widget.primarySelection = false;
    }
}

std::optional<std::uint64_t> WidgetScene::PrimarySelection() const noexcept {
    for (const auto& widget : widgets_) {
        if (widget.selected && widget.primarySelection) {
            return widget.id;
        }
    }
    return std::nullopt;
}

std::size_t WidgetScene::SelectionCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(widgets_.begin(), widgets_.end(), [](const WidgetInstance& widget) {
        return widget.selected;
    }));
}

} // namespace ws
