#include "scene/WidgetScene.h"

#include "layout/OuterLayout.h"
#include "widgets/WidgetRegistry.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <sstream>

namespace ws {
namespace {

std::string GenerateInstanceId() {
    static std::atomic_uint64_t sequence{0};
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream stream;
    stream << "widget-" << std::hex << ticks << '-' << sequence.fetch_add(1, std::memory_order_relaxed);
    return stream.str();
}

bool Overlaps(const GridPlacement& left, const GridPlacement& right) noexcept {
    return left.column < right.column + right.columnSpan &&
        left.column + left.columnSpan > right.column &&
        left.row < right.row + right.rowSpan &&
        left.row + left.rowSpan > right.row;
}

} // namespace

WidgetScene::WidgetScene(const WidgetRegistry& registry) : registry_(registry) {}

WidgetInstance* WidgetScene::Find(std::string_view instanceId) noexcept {
    const auto it = std::find_if(widgets_.begin(), widgets_.end(), [instanceId](const WidgetInstance& widget) {
        return widget.instanceId == instanceId;
    });
    return it == widgets_.end() ? nullptr : &*it;
}

const WidgetInstance* WidgetScene::Find(std::string_view instanceId) const noexcept {
    const auto it = std::find_if(widgets_.begin(), widgets_.end(), [instanceId](const WidgetInstance& widget) {
        return widget.instanceId == instanceId;
    });
    return it == widgets_.end() ? nullptr : &*it;
}

std::optional<std::string> WidgetScene::HitTest(
    PointF point, const GridLayout& layout, const GridMetrics& metrics) const noexcept {
    for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it) {
        if (OuterLayout::RectFor(*it, layout, metrics).Contains(point)) return it->instanceId;
    }
    return std::nullopt;
}

void WidgetScene::Select(std::string_view instanceId, bool additive) noexcept {
    if (!additive) ClearSelection();
    WidgetInstance* target = Find(instanceId);
    if (!target) return;
    if (additive && target->selected) {
        target->selected = false;
        target->primarySelection = false;
        for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it) {
            if (it->selected) { it->primarySelection = true; break; }
        }
        return;
    }
    for (auto& widget : widgets_) widget.primarySelection = false;
    target->selected = true;
    target->primarySelection = true;
}

void WidgetScene::ClearSelection() noexcept {
    for (auto& widget : widgets_) {
        widget.selected = false;
        widget.primarySelection = false;
    }
}

std::optional<std::string> WidgetScene::PrimarySelection() const {
    for (const auto& widget : widgets_) {
        if (widget.selected && widget.primarySelection) return widget.instanceId;
    }
    return std::nullopt;
}

std::size_t WidgetScene::SelectionCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(widgets_.begin(), widgets_.end(),
        [](const WidgetInstance& widget) { return widget.selected; }));
}

GridPlacement WidgetScene::FindInitialPlacement(
    GridSize size, std::wstring_view monitorId) const noexcept {
    const int spanColumns = std::clamp(size.columns, 1, gridColumns_);
    const int spanRows = std::clamp(size.rows, 1, gridRows_);
    for (int row = 0; row <= gridRows_ - spanRows; ++row) {
        for (int column = 0; column <= gridColumns_ - spanColumns; ++column) {
            const GridPlacement candidate{column, row, spanColumns, spanRows};
            const bool occupied = std::any_of(widgets_.begin(), widgets_.end(), [&](const WidgetInstance& widget) {
                return widget.monitorId == monitorId && widget.layoutMode == LayoutMode::Grid &&
                    Overlaps(candidate, widget.grid);
            });
            if (!occupied) return candidate;
        }
    }
    return GridPlacement{0, 0, spanColumns, spanRows};
}

WidgetInstance* WidgetScene::CreateWidget(std::string_view typeId, std::wstring_view monitorId) {
    const WidgetDescriptor* descriptor = registry_.Find(typeId);
    std::unique_ptr<IWidget> content = registry_.Create(typeId);
    if (!descriptor || !content) return nullptr;
    WidgetInstance instance{};
    instance.instanceId = GenerateInstanceId();
    instance.typeId = descriptor->typeId;
    instance.monitorId = monitorId;
    instance.grid = FindInitialPlacement(descriptor->defaultGridSize, monitorId);
    instance.content = std::move(content);
    widgets_.push_back(std::move(instance));
    Select(widgets_.back().instanceId, false);
    return &widgets_.back();
}

bool WidgetScene::RemoveWidget(std::string_view instanceId) noexcept {
    const auto found = std::find_if(widgets_.begin(), widgets_.end(), [instanceId](const WidgetInstance& widget) {
        return widget.instanceId == instanceId;
    });
    if (found == widgets_.end()) return false;
    const bool removedPrimary = found->primarySelection;
    widgets_.erase(found);
    if (removedPrimary) {
        for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it) {
            if (it->selected) { it->primarySelection = true; break; }
        }
    }
    return true;
}

std::size_t WidgetScene::RemoveSelectedWidgets() noexcept {
    const auto oldSize = widgets_.size();
    std::erase_if(widgets_, [](const WidgetInstance& widget) { return widget.selected; });
    return oldSize - widgets_.size();
}

WidgetInstance* WidgetScene::DuplicateWidget(std::string_view instanceId) {
    const WidgetInstance* source = Find(instanceId);
    if (!source) return nullptr;
    WidgetPersistenceRecord record{};
    record.typeId = source->typeId;
    record.monitorId = source->monitorId;
    record.layoutMode = source->layoutMode;
    record.grid = FindInitialPlacement(
        GridSize{source->grid.columnSpan, source->grid.rowSpan}, source->monitorId);
    record.free = source->free;
    record.locked = source->locked;
    record.contentScale = source->contentScale;
    record.appearance = source->appearance;
    record.widgetState = source->content->SaveState();
    return RestoreWidget(record, true);
}

bool WidgetScene::SetWidgetLocked(std::string_view instanceId, bool locked) noexcept {
    WidgetInstance* widget = Find(instanceId);
    if (!widget) return false;
    widget->locked = locked;
    return true;
}

bool WidgetScene::SetWidgetLayoutMode(
    std::string_view instanceId, LayoutMode mode, const GridLayout& grid, const GridMetrics& metrics) noexcept {
    WidgetInstance* widget = Find(instanceId);
    if (!widget || widget->layoutMode == mode) return widget != nullptr;
    const RectF current = OuterLayout::RectFor(*widget, grid, metrics);
    if (mode == LayoutMode::Free) {
        widget->free = FreePlacement{current.x, current.y, current.width, current.height};
    } else {
        const float stride = std::max(1.0f, metrics.cellSize + metrics.gap);
        widget->grid.columnSpan = std::clamp(
            static_cast<int>(std::lround((current.width + metrics.gap) / stride)), 1, gridColumns_);
        widget->grid.rowSpan = std::clamp(
            static_cast<int>(std::lround((current.height + metrics.gap) / stride)), 1, gridRows_);
        widget->grid = grid.MoveToPoint(widget->grid, PointF{current.x, current.y}, PointF{}, metrics);
    }
    widget->layoutMode = mode;
    return true;
}

bool WidgetScene::AlignSelected(AlignmentOperation operation, RectF bounds) noexcept {
    std::vector<AlignmentItem> items;
    for (auto& widget : widgets_) {
        if (widget.selected && widget.layoutMode == LayoutMode::Free) {
            items.push_back(AlignmentItem{&widget.free, widget.primarySelection, widget.locked});
        }
    }
    return Alignment::Apply(items, operation, bounds);
}

WidgetSceneSnapshot WidgetScene::Snapshot() const {
    WidgetSceneSnapshot snapshot;
    snapshot.reserve(widgets_.size());
    for (const auto& widget : widgets_) {
        snapshot.push_back(WidgetPersistenceRecord{
            .instanceId = widget.instanceId, .typeId = widget.typeId, .monitorId = widget.monitorId,
            .layoutMode = widget.layoutMode, .grid = widget.grid, .free = widget.free,
            .locked = widget.locked, .contentScale = widget.contentScale,
            .appearance = widget.appearance, .widgetState = widget.content->SaveState(),
        });
    }
    return snapshot;
}

WidgetInstance* WidgetScene::RestoreWidget(const WidgetPersistenceRecord& record, bool select) {
    if (!record.instanceId.empty() && Find(record.instanceId)) return nullptr;
    std::unique_ptr<IWidget> content = registry_.Create(record.typeId);
    if (!content) return nullptr;
    content->RestoreState(record.widgetState);
    WidgetInstance instance{};
    instance.instanceId = record.instanceId.empty() ? GenerateInstanceId() : record.instanceId;
    instance.typeId = record.typeId;
    instance.monitorId = record.monitorId.empty() ? L"primary" : record.monitorId;
    instance.layoutMode = record.layoutMode;
    instance.grid = record.grid;
    instance.grid.columnSpan = std::clamp(instance.grid.columnSpan, 1, gridColumns_);
    instance.grid.rowSpan = std::clamp(instance.grid.rowSpan, 1, gridRows_);
    instance.grid.column = std::clamp(instance.grid.column, 0, gridColumns_ - instance.grid.columnSpan);
    instance.grid.row = std::clamp(instance.grid.row, 0, gridRows_ - instance.grid.rowSpan);
    instance.free = record.free;
    instance.free.width = std::max(1.0f, instance.free.width);
    instance.free.height = std::max(1.0f, instance.free.height);
    instance.locked = record.locked;
    instance.contentScale = std::clamp(record.contentScale, 0.25f, 4.0f);
    instance.appearance = record.appearance;
    instance.appearance.opacity = std::clamp(instance.appearance.opacity, 0.0f, 1.0f);
    instance.appearance.blurRadius = std::clamp(instance.appearance.blurRadius, 0.0f, 128.0f);
    instance.appearance.cornerRadius = std::clamp(instance.appearance.cornerRadius, 0.0f, 128.0f);
    instance.content = std::move(content);
    widgets_.push_back(std::move(instance));
    if (select) Select(widgets_.back().instanceId, false);
    return &widgets_.back();
}

} // namespace ws
