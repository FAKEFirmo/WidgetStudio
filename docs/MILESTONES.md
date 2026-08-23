# Planned milestones

## M1 - Foundation (complete)

- Win32 app shell and tray controller
- Direct2D / DirectWrite / WIC
- wallpaper background
- square grid
- scene selection and Shift multi-selection
- snapped dragging and locking
- edit/passive mode

## M2 - Widget Framework and Widget Library (implemented, build validation pending)

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

Clock, Calendar, Music, and Photo are not implemented in this milestone.

## M3 - Local persistence (implemented, build validation pending)

- versioned JSON scene schema without a third-party library
- malformed/unsupported configuration rejection
- atomic temporary-file write and Win32 replacement
- previous-configuration backup
- `%LOCALAPPDATA%\WidgetStudio` default location
- opt-in portable-data mode beside the executable
- event-driven saves after create, delete, duplicate, lock, and completed drag
- unknown widget-type record preservation
- focused codec, store, placement, ID, lifecycle, and restore tests

## M4 - Authored layout + production Clock (implemented, build validation pending)

- authored reference-layout engine
- geometric centering after uniform scale
- shared glass style object
- common vector/text/image helpers
- production Clock widget registered through the framework
- aspect-profile selection with breakpoint hysteresis
- uniform fit and explicit centering logic tests
- dark/light shared card surface, border, padding, and shadow primitives
- one-shot event-driven widget update scheduling
- Clock 12/24-hour mode, optional seconds/divider, and date-format state

## M5 - Widget Studio

- settings window using the same WidgetScene
- universal and widget-provided settings panels
- grid sizing, alignment, and distribution
- dark/light/glass settings

## M6 - Calendar + Photo

- production Calendar
- WIC photo import and local asset library
- cover/contain and focal point

## M7 - Music

- Windows Global System Media Transport Controls integration
- metadata/artwork
- Previous / Play-Pause / Next and progress timeline
- authored Ultra-wide / Landscape / Square / Portrait layouts

## M8 - Desktop integration

- `IDesktopBackend`
- stable desktop-host behavior
- passive hit testing
- optional isolated WorkerW backend
- Explorer restart recovery

## M9 - Multi-monitor + polish

- per-monitor scenes/grids and monitor migration
- event-driven invalidation
- wallpaper/cache refresh
- startup option and packaging
