#include "widgets/BuiltInWidgets.h"

#include "persistence/SceneStore.h"
#include "widgets/CalendarWidget.h"
#include "widgets/ClockWidget.h"
#include "widgets/DebugWidget.h"
#include "widgets/MusicWidget.h"
#include "widgets/PhotoWidget.h"
#include "widgets/WidgetRegistry.h"

#include <utility>

namespace ws {

bool RegisterBuiltInWidgets(WidgetRegistry& registry, std::shared_ptr<MediaSessionService> mediaSession) {
    bool registered = true;
#ifdef _DEBUG
    registered = registry.Register(DebugWidget::Descriptor());
#endif
    return registered && registry.Register(ClockWidget::Descriptor()) &&
        registry.Register(CalendarWidget::Descriptor()) &&
        registry.Register(PhotoWidget::Descriptor(SceneStore::DefaultImageDirectory())) &&
        registry.Register(MusicWidget::Descriptor(std::move(mediaSession)));
}

} // namespace ws
