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
    const std::filesystem::path assetDirectory =
        SceneStore::DefaultConfigPath().parent_path() / L"assets";
    return registry.Register(DebugWidget::Descriptor()) &&
        registry.Register(ClockWidget::Descriptor()) &&
        registry.Register(CalendarWidget::Descriptor()) &&
        registry.Register(PhotoWidget::Descriptor(assetDirectory)) &&
        registry.Register(MusicWidget::Descriptor(std::move(mediaSession)));
}

} // namespace ws
