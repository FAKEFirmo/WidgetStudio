# Planned milestones

## M1 - Foundation (complete)

- Win32 app shell and tray controller
- Direct2D / DirectWrite / WIC
- wallpaper background
- square grid
- scene selection and Shift multi-selection
- snapped dragging and locking
- edit/passive mode

## M2 - Widget Framework and Widget Library (current)

- separate widget type descriptors from widget instances
- stable type and instance IDs
- narrow `IWidget` content/settings/state interface
- explicit application-startup registration in `WidgetRegistry`
- generic create, remove, duplicate, and lock lifecycle operations
- first-free grid placement with a clamped full-grid fallback
- registry-driven native Widget Library through **Tray > Add Widget...**
- DebugWidget for multiple-instance lifecycle validation
- Delete, Ctrl+D, and Ctrl+L edit commands
- encoding-neutral persistence records and registry-based restore path

File encoding and final settings UI are intentionally deferred. Clock, Calendar, Music, and Photo are not implemented in this milestone.

## M3 - Authored layout + production Clock

- authored reference-layout engine
- geometric centering after uniform scale
- shared glass style object
- common vector/text/image helpers
- production Clock widget registered through the framework

## M4 - Widget Studio + persistence

- settings window using the same WidgetScene
- universal and widget-provided settings panels
- grid sizing, alignment, and distribution
- dark/light/glass settings
- local scene encoding under `%LOCALAPPDATA%`

## M5 - Calendar + Photo

- production Calendar
- WIC photo import and local asset library
- cover/contain and focal point

## M6 - Music

- Windows Global System Media Transport Controls integration
- metadata/artwork
- Previous / Play-Pause / Next and progress timeline
- authored Ultra-wide / Landscape / Square / Portrait layouts

## M7 - Desktop integration

- `IDesktopBackend`
- stable desktop-host behavior
- passive hit testing
- optional isolated WorkerW backend
- Explorer restart recovery

## M8 - Multi-monitor + polish

- per-monitor scenes/grids and monitor migration
- event-driven invalidation
- wallpaper/cache refresh
- startup option and packaging
