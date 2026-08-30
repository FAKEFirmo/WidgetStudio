# Planned milestones

## M1 - Foundation (complete)

- Win32 app shell and tray controller
- Direct2D / DirectWrite / WIC
- wallpaper background
- square grid
- scene selection and Shift multi-selection
- snapped dragging and locking
- edit/passive mode

## M2 - Widget Framework and Widget Library (implemented; native build validation pending)

- separate widget type descriptors from widget instances
- stable type and instance IDs
- narrow `IWidget` content/settings/state interface
- explicit application-startup registration in `WidgetRegistry`
- generic create, remove, duplicate, and lock lifecycle operations
- first-free grid placement with a clamped full-grid fallback
- registry-driven native Widget Library through **Tray > Add Widget...**
- Debug-only Widget for multiple-instance lifecycle validation, excluded from Release
- Delete, Ctrl+D, and Ctrl+L edit commands
- encoding-neutral persistence records and registry-based restore path

## M3 - Local persistence (implemented; native build validation pending)

- versioned JSON scene schema without a third-party library
- malformed/unsupported configuration rejection
- atomic temporary-file write and Win32 replacement
- previous-configuration backup
- `%LOCALAPPDATA%\WidgetStudio` default location
- opt-in `data\config`, `data\images`, and `data\cache` hierarchy beside the executable
- event-driven saves after create, delete, duplicate, lock, and completed drag
- unknown widget-type record preservation
- focused codec, store, placement, ID, lifecycle, and restore tests

## M4 - Authored layout + production Clock (implemented; native build validation pending)

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

## M5 - Widget Studio (implemented; native build validation pending)

- settings window using the same WidgetScene
- universal and widget-provided settings panels
- grid sizing, alignment, and distribution
- dark/light/glass settings
- live fitted desktop preview over the shared scene and renderer
- native boolean, choice, number/text, and local-file setting editors
- add, duplicate, remove, lock, and multi-selection operations

The underlying free-mode rectangle, drag, conversion, primary-relative alignment/matching, and distribution logic is implemented ahead of this UI milestone because interactive Music controls depend on the same generic interaction separation.

## M6 - Calendar + Photo (implemented; native build validation pending)

- production Calendar
- WIC photo import and local asset library
- cover/contain and focal point
- localized month/year and weekday labels
- Monday/Sunday start, weekend dimming, adjacent dates, and today highlight
- local-midnight Calendar update scheduling
- WIC bitmap decoding and render-target-generation cache invalidation
- pure proportional fill/fit and continuous focal-point math
- application-owned atomic local asset import

## M7 - Music (implemented; native build validation pending)

- Windows Global System Media Transport Controls integration
- metadata/artwork
- Previous / Play-Pause / Next and progress timeline
- authored Ultra-wide / Landscape / Square / Portrait layouts
- shared event-driven Windows media-session service
- cached media snapshot and artwork revision handling
- square proportional artwork decoding through WIC
- one progress bar, elapsed and negative-remaining time
- vector Previous / Play-Pause / Next controls through generic action hit testing
- 500 ms one-shot progress refresh only while playing
- dedicated event/command-driven MTA worker for WinRT media operations

## M8 - Desktop integration (implemented; Windows 11 desktop validation pending)

- `IDesktopBackend`
- stable desktop-host behavior
- passive hit testing
- one lightweight HWND per widget with a hidden controller HWND
- default WorkerW attachment with safe windowed fallback
- Explorer restart recovery
- process-local diagnostic fallback selection
- tray icon restoration after Explorer restarts

## M9 - Multi-monitor + polish (implemented; runtime validation pending)

- per-monitor scenes/grids and monitor migration
- event-driven invalidation
- wallpaper/cache refresh
- startup option and packaging
- monitor-scoped rendering and hit testing
- simultaneous per-widget windows sharing one scene and process
- tested monitor-DIP to physical-pixel widget-window placement
- effective-DPI/work-area enumeration and missing-monitor migration
- Per-Monitor V2 manifest, version resource, static runtime, and portable CMake install
- opt-in removable Startup-folder shortcut with no registry configuration

## Validation status

Pure-logic coverage is present for registry lifecycle, ID generation, monitor-isolated hit testing, grid placement, authored layout selection and fitting, scene serialization and restore, atomic persistence, alignment/distribution, Clock scheduling, Calendar generation, Photo fit/fill/focal math, and asset import.

The current per-widget-window milestone compiles and links warning-clean with MSVC 19.51 through a direct compiler validation path. The logic-test executable passes, including monitor/DPI window-placement coverage. A portable Debug smoke run remained alive, used exactly one WidgetStudio process, created its clock instance, and wrote `data\config\scene.json`. Native enumeration confirmed separate `WidgetStudioHostWindow` and `WidgetStudioDesktopWidgetWindow` HWNDs. Explorer did not expose a WorkerW in the validation session, exercising the safe top-level fallback.

The current CMake/Ninja validation is pending because enterprise Application Control blocks CLion's bundled `cmake.exe` and `ninja.exe`. It no longer blocks executables produced directly by the installed Microsoft compiler. The policy has not been weakened; installing Visual Studio's optional Microsoft-signed CMake tools or organization-allowing the CLion tools is the remaining build-workflow prerequisite. Windows 11 Explorer attachment, interactive UI, multi-monitor, Release packaging, and idle performance still require final validation.
