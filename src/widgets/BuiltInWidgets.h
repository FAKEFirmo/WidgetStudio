#pragma once

#include <memory>

namespace ws {

class WidgetRegistry;
class MediaSessionService;

bool RegisterBuiltInWidgets(WidgetRegistry& registry, std::shared_ptr<MediaSessionService> mediaSession);

} // namespace ws
