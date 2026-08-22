# Planned milestones

## M1 - Foundation (this package)
- Win32 app shell
- Direct2D / DirectWrite / WIC
- wallpaper background
- square grid
- scene model
- selection and multi-selection
- snapped dragging
- edit-mode state
- tray control

## M2 - Shared widget runtime
- `IWidget` interface
- authored reference-layout engine
- geometric centering after scale
- shared glass style object
- common vector/text/image helpers
- production Clock widget

## M3 - Widget Studio + persistence
- settings window using the same WidgetScene
- selection-driven settings panels
- grid sizing
- alignment and distribution
- dark/light/glass settings
- JSON persistence under `%LOCALAPPDATA%`

## M4 - Calendar + Photo
- production Calendar
- WIC photo import
- local asset library
- cover/contain
- focal point

## M5 - Music
- Windows Global System Media Transport Controls integration
- metadata/artwork
- Previous / Play-Pause / Next
- progress timeline
- authored Ultra-wide / Landscape / Square / Portrait layouts

## M6 - Desktop integration
- `IDesktopBackend`
- stable desktop-host behavior
- passive hit testing
- optional isolated WorkerW backend
- Explorer restart recovery

## M7 - Multi-monitor + polish
- per-monitor scenes/grids
- monitor migration
- event-driven invalidation
- wallpaper/cache refresh
- startup option
- packaging
