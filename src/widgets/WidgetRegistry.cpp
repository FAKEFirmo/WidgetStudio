#include "widgets/WidgetRegistry.h"

#include "widgets/IWidget.h"

#include <algorithm>
#include <utility>

namespace ws {

bool WidgetRegistry::Register(WidgetDescriptor descriptor) {
    const auto validSize = [](GridSize size) { return size.columns > 0 && size.rows > 0; };
    const bool orderedSizes = descriptor.minimumGridSize.columns <= descriptor.defaultGridSize.columns &&
        descriptor.defaultGridSize.columns <= descriptor.maximumGridSize.columns &&
        descriptor.minimumGridSize.rows <= descriptor.defaultGridSize.rows &&
        descriptor.defaultGridSize.rows <= descriptor.maximumGridSize.rows;
    if (descriptor.typeId.empty() || descriptor.displayName.empty() || !descriptor.factory ||
        !validSize(descriptor.minimumGridSize) || !validSize(descriptor.defaultGridSize) ||
        !validSize(descriptor.maximumGridSize) || !orderedSizes || Find(descriptor.typeId)) return false;
    descriptors_.push_back(std::move(descriptor));
    return true;
}

const WidgetDescriptor* WidgetRegistry::Find(std::string_view typeId) const noexcept {
    const auto found = std::find_if(descriptors_.begin(), descriptors_.end(),
        [typeId](const WidgetDescriptor& descriptor) { return descriptor.typeId == typeId; });
    return found == descriptors_.end() ? nullptr : &*found;
}

std::unique_ptr<IWidget> WidgetRegistry::Create(std::string_view typeId) const {
    const WidgetDescriptor* descriptor = Find(typeId);
    if (!descriptor) return nullptr;
    return descriptor->factory();
}

} // namespace ws
