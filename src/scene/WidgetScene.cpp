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

bool Overlaps(const FreePlacement& left, const FreePlacement& right) noexcept {
    return left.x < right.x + right.width && left.x + left.width > right.x &&
        left.y < right.y + right.height && left.y + left.height > right.y;
}

WidgetState EffectiveWidgetState(const WidgetInstance& widget) {
    WidgetState state = widget.preservedWidgetState;
    if (!widget.content) return state;
    for (auto& [key, value] : widget.content->SaveState()) {
        state.insert_or_assign(std::move(key), std::move(value));
    }
    return state;
}

} // namespace

WidgetScene::WidgetScene(const WidgetRegistry& registry) : registry_(registry) {}

namespace {

WidgetAppearance SanitizeAppearance(WidgetAppearance appearance) {
    if (!std::isfinite(appearance.opacity)) appearance.opacity = 0.62f;
    if (!std::isfinite(appearance.blurRadius)) appearance.blurRadius = 18.0f;
    if (!std::isfinite(appearance.cornerRadius)) appearance.cornerRadius = 24.0f;
    if (!std::isfinite(appearance.innerPadding)) appearance.innerPadding = 20.0f;
    appearance.opacity = std::clamp(appearance.opacity, 0.0f, 1.0f);
    appearance.blurRadius = std::clamp(appearance.blurRadius, 0.0f, 128.0f);
    appearance.cornerRadius = std::clamp(appearance.cornerRadius, 0.0f, 128.0f);
    appearance.innerPadding = std::clamp(appearance.innerPadding, 0.0f, 64.0f);
    appearance.glassEnabled = appearance.surface == SurfaceMode::Frosted;
    if (appearance.fontFamily.empty() || appearance.fontFamily.size() > 128) {
        appearance.fontFamily = L"Segoe UI Variable";
    }
    appearance.tintColor &= 0x00FFFFFFu;
    return appearance;
}

} // namespace

void WidgetScene::SetGeneralAppearance(WidgetAppearance appearance) {
    generalAppearance_ = SanitizeAppearance(std::move(appearance));
    for (WidgetInstance& widget : widgets_) {
        if (widget.useGeneralAppearance) widget.appearance = generalAppearance_;
    }
}

bool WidgetScene::SetUseGeneralAppearance(std::string_view instanceId, bool enabled) {
    WidgetInstance* widget = Find(instanceId);
    if (!widget) return false;
    widget->useGeneralAppearance = enabled;
    if (enabled) widget->appearance = generalAppearance_;
    return true;
}

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

const WidgetDescriptor* WidgetScene::DescriptorFor(std::string_view instanceId) const noexcept {
    const WidgetInstance* widget = Find(instanceId);
    return widget ? registry_.Find(widget->typeId) : nullptr;
}

std::optional<std::string> WidgetScene::HitTest(
    PointF point, const GridLayout& layout, const GridMetrics& metrics,
    std::wstring_view monitorId) const noexcept {
    for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it) {
        if (!monitorId.empty() && it->monitorId != monitorId) continue;
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
    instance.appearance = generalAppearance_;
    instance.useGeneralAppearance = true;
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

FreePlacement WidgetScene::FindDuplicateFreePlacement(
    const WidgetInstance& source, RectF bounds) const noexcept {
    constexpr float offset = 24.0f;
    FreePlacement candidate = source.free;
    if (!std::isfinite(candidate.x) || !std::isfinite(candidate.y) ||
        !std::isfinite(candidate.width) || !std::isfinite(candidate.height)) {
        candidate = FreePlacement{0.0f, 0.0f, 320.0f, 180.0f};
    }
    candidate.width = std::max(1.0f, candidate.width);
    candidate.height = std::max(1.0f, candidate.height);

    const bool bounded = std::isfinite(bounds.x) && std::isfinite(bounds.y) &&
        std::isfinite(bounds.width) && std::isfinite(bounds.height) &&
        bounds.width > 0.0f && bounds.height > 0.0f;
    if (!bounded) {
        candidate.x += offset;
        candidate.y += offset;
        return candidate;
    }

    candidate.width = std::min(candidate.width, bounds.width);
    candidate.height = std::min(candidate.height, bounds.height);
    const float maximumX = bounds.Right() - candidate.width;
    const float maximumY = bounds.Bottom() - candidate.height;
    const auto isFree = [this, &source](const FreePlacement& placement) {
        return std::none_of(widgets_.begin(), widgets_.end(), [&](const WidgetInstance& widget) {
            return widget.monitorId == source.monitorId && widget.layoutMode == LayoutMode::Free &&
                Overlaps(placement, widget.free);
        });
    };

    const PointF nearby[] = {
        {source.free.x + candidate.width + offset, source.free.y},
        {source.free.x, source.free.y + candidate.height + offset},
        {source.free.x - candidate.width - offset, source.free.y},
        {source.free.x, source.free.y - candidate.height - offset},
    };
    for (const PointF point : nearby) {
        candidate.x = std::clamp(point.x, bounds.x, maximumX);
        candidate.y = std::clamp(point.y, bounds.y, maximumY);
        if (isFree(candidate)) return candidate;
    }

    // Search a deterministic DIP lattice when the nearby offset is occupied.
    // Include the far edges explicitly so every returned rectangle is valid.
    for (float y = bounds.y; y <= maximumY; y += offset) {
        for (float x = bounds.x; x <= maximumX; x += offset) {
            candidate.x = x;
            candidate.y = y;
            if (isFree(candidate)) return candidate;
        }
    }
    candidate.x = maximumX;
    candidate.y = maximumY;
    if (isFree(candidate)) return candidate;

    // A completely full free-layout surface necessarily overlaps; keep the
    // fallback bounded and deterministic rather than reproducing the source.
    candidate.x = std::clamp(source.free.x + offset, bounds.x, maximumX);
    candidate.y = std::clamp(source.free.y + offset, bounds.y, maximumY);
    return candidate;
}

WidgetInstance* WidgetScene::DuplicateWidget(std::string_view instanceId, RectF bounds) {
    const WidgetInstance* source = Find(instanceId);
    if (!source) return nullptr;
    const WidgetDescriptor* descriptor = registry_.Find(source->typeId);
    if (!descriptor || !HasCapability(descriptor->capabilities, WidgetCapability::Duplicatable)) return nullptr;
    WidgetPersistenceRecord record{};
    record.typeId = source->typeId;
    record.monitorId = source->monitorId;
    record.layoutMode = source->layoutMode;
    record.grid = FindInitialPlacement(
        GridSize{source->grid.columnSpan, source->grid.rowSpan}, source->monitorId);
    record.free = FindDuplicateFreePlacement(*source, bounds);
    record.locked = source->locked;
    record.contentScale = source->contentScale;
    record.appearance = source->appearance;
    record.useGeneralAppearance = source->useGeneralAppearance;
    record.widgetState = EffectiveWidgetState(*source);
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
        const WidgetDescriptor* descriptor = registry_.Find(widget->typeId);
        const int minimumColumns = std::min(gridColumns_, descriptor ? descriptor->minimumGridSize.columns : 1);
        const int minimumRows = std::min(gridRows_, descriptor ? descriptor->minimumGridSize.rows : 1);
        const int maximumColumns = std::max(minimumColumns,
            std::min(gridColumns_, descriptor ? descriptor->maximumGridSize.columns : gridColumns_));
        const int maximumRows = std::max(minimumRows,
            std::min(gridRows_, descriptor ? descriptor->maximumGridSize.rows : gridRows_));
        widget->grid = OuterLayout::GridForRect(current, widget->grid, grid, metrics,
            minimumColumns, minimumRows, maximumColumns, maximumRows);
    }
    widget->layoutMode = mode;
    return true;
}

bool WidgetScene::AlignSelected(
    AlignmentOperation operation, RectF bounds, std::wstring_view monitorId) noexcept {
    std::vector<AlignmentItem> items;
    for (auto& widget : widgets_) {
        if (widget.selected && widget.layoutMode == LayoutMode::Free &&
            (monitorId.empty() || widget.monitorId == monitorId)) {
            items.push_back(AlignmentItem{&widget.free, widget.primarySelection, widget.locked});
        }
    }
    return Alignment::Apply(items, operation, bounds);
}

WidgetSceneSnapshot WidgetScene::Snapshot() const {
    WidgetSceneSnapshot snapshot;
    snapshot.generalAppearance = generalAppearance_;
    snapshot.reserve(widgets_.size());
    for (const auto& widget : widgets_) {
        snapshot.push_back(WidgetPersistenceRecord{
            .instanceId = widget.instanceId, .typeId = widget.typeId, .monitorId = widget.monitorId,
            .layoutMode = widget.layoutMode, .grid = widget.grid, .free = widget.free,
            .locked = widget.locked, .contentScale = widget.contentScale,
            .appearance = widget.appearance, .useGeneralAppearance = widget.useGeneralAppearance,
            .widgetState = EffectiveWidgetState(widget),
        });
    }
    return snapshot;
}

WidgetInstance* WidgetScene::RestoreWidget(const WidgetPersistenceRecord& record, bool select) {
    if (!record.instanceId.empty() && Find(record.instanceId)) return nullptr;
    const WidgetDescriptor* descriptor = registry_.Find(record.typeId);
    if (!descriptor) return nullptr;
    std::unique_ptr<IWidget> content = registry_.Create(record.typeId);
    if (!content) return nullptr;
    content->RestoreState(record.widgetState);
    WidgetInstance instance{};
    instance.instanceId = record.instanceId.empty() ? GenerateInstanceId() : record.instanceId;
    instance.typeId = record.typeId;
    instance.monitorId = record.monitorId.empty() ? L"primary" : record.monitorId;
    instance.layoutMode = record.layoutMode;
    instance.grid = record.grid;
    const int minimumColumns = std::min(gridColumns_, descriptor->minimumGridSize.columns);
    const int minimumRows = std::min(gridRows_, descriptor->minimumGridSize.rows);
    const int maximumColumns = std::max(minimumColumns,
        std::min(gridColumns_, descriptor->maximumGridSize.columns));
    const int maximumRows = std::max(minimumRows,
        std::min(gridRows_, descriptor->maximumGridSize.rows));
    instance.grid.columnSpan = std::clamp(instance.grid.columnSpan, minimumColumns, maximumColumns);
    instance.grid.rowSpan = std::clamp(instance.grid.rowSpan, minimumRows, maximumRows);
    instance.grid.column = std::clamp(instance.grid.column, 0, gridColumns_ - instance.grid.columnSpan);
    instance.grid.row = std::clamp(instance.grid.row, 0, gridRows_ - instance.grid.rowSpan);
    instance.free = record.free;
    if (!std::isfinite(instance.free.x)) instance.free.x = 0.0f;
    if (!std::isfinite(instance.free.y)) instance.free.y = 0.0f;
    if (!std::isfinite(instance.free.width) || instance.free.width <= 0.0f) instance.free.width = 320.0f;
    if (!std::isfinite(instance.free.height) || instance.free.height <= 0.0f) instance.free.height = 180.0f;
    instance.locked = record.locked;
    instance.contentScale = std::isfinite(record.contentScale)
        ? std::clamp(record.contentScale, 0.25f, 4.0f) : 1.0f;
    instance.useGeneralAppearance = record.useGeneralAppearance;
    instance.appearance = instance.useGeneralAppearance ? generalAppearance_ : record.appearance;
    if (instance.appearance.surface == SurfaceMode::Frosted && !instance.appearance.glassEnabled) {
        instance.appearance.surface = SurfaceMode::Solid;
    }
    instance.appearance = SanitizeAppearance(std::move(instance.appearance));
    instance.preservedWidgetState = record.widgetState;
    instance.content = std::move(content);
    widgets_.push_back(std::move(instance));
    if (select) Select(widgets_.back().instanceId, false);
    return &widgets_.back();
}

} // namespace ws
