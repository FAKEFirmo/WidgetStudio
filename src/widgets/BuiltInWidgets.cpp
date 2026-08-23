#include "widgets/BuiltInWidgets.h"

#include "widgets/ClockWidget.h"
#include "widgets/DebugWidget.h"
#include "widgets/WidgetRegistry.h"

namespace ws {

bool RegisterBuiltInWidgets(WidgetRegistry& registry) {
    return registry.Register(DebugWidget::Descriptor()) &&
        registry.Register(ClockWidget::Descriptor());
}

} // namespace ws
