# Planned milestones

## M1 - Foundation (complete)

- Win32 app shell and tray controller
- Direct2D / DirectWrite / WIC
- wallpaper background
- square grid
- scene selection and Shift multi-selection
- snapped dragging and locking
- edit/passive mode

## M2 - Widget Framework and Widget Library (implemented; clean CMake build validated)

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

## M3 - Local persistence (implemented; clean CMake build validated)

- versioned JSON scene schema without a third-party library
- malformed/unsupported configuration rejection
- atomic temporary-file write and Win32 replacement
- previous-configuration backup
- `%LOCALAPPDATA%\WidgetStudio` default location
- opt-in `data\config`, `data\images`, and `data\cache` hierarchy beside the executable
- event-driven saves after create, delete, duplicate, lock, and completed drag
- unknown widget-type record preservation
- focused codec, store, placement, ID, lifecycle, and restore tests

## M4 - Authored layout + production Clock (implemented; clean CMake build validated)

- authored reference-layout engine
- geometric centering after uniform scale
- shared glass style object
- common vector/text/image helpers
- production Clock widget registered through the framework
- aspect-profile selection with breakpoint hysteresis
- uniform fit and explicit centering logic tests
- dark/light shared card surface, border, padding, and shadow primitives
- one-shot event-driven widget update scheduling
- Clock 12/24-hour mode, optional seconds/date/divider, date-format state, and font choice

## M5 - Widget Studio (implemented; clean CMake build validated)

- settings window using the same WidgetScene
- universal and widget-provided settings panels
- grid sizing, alignment, and distribution
- dark/light/glass settings
- live fitted desktop preview over the shared scene and renderer
- native boolean, choice, number/text, and local-file setting editors
- add, duplicate, remove, lock, and multi-selection operations
- capability-aware scaling, resizing, duplication, configuration, and passive hit testing
- topology-backed monitor assignment with destination work-area clamping

The underlying free-mode rectangle, drag, conversion, primary-relative alignment/matching, and distribution logic is implemented ahead of this UI milestone because interactive Music controls depend on the same generic interaction separation.

## M6 - Calendar + Photo (implemented; clean CMake build validated)

- production Calendar
- WIC photo import and local asset library
- cover/contain and focal point
- localized month/year and weekday labels
- Monday/Sunday start, weekend dimming, adjacent dates, and today highlight
- local-midnight Calendar update scheduling
- WIC bitmap decoding and independent desktop/preview target caches with generation invalidation
- pure proportional fill/fit and continuous focal-point math
- application-owned atomic local asset import

## M7 - Music (implemented; clean CMake build validated)

- Windows Global System Media Transport Controls integration
- metadata/artwork
- Previous / Play-Pause / Next and progress timeline
- authored Ultra-wide / Landscape / Square / Portrait layouts
- shared event-driven Windows media-session service
- cached media snapshot and per-render-target artwork revision handling
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
- one process-shared Direct2D/DirectWrite/WIC factory set and wallpaper decode with per-widget device-region caches
- wallpaper/cache refresh
- startup option and packaging
- monitor-scoped rendering and hit testing
- simultaneous per-widget windows sharing one scene and process
- tested monitor-DIP to physical-pixel widget-window placement
- effective-DPI/work-area enumeration, missing-monitor migration, and resolution/work-area reconciliation
- Per-Monitor V2 manifest, version resource, static runtime, and portable CMake install
- opt-in removable Startup-folder shortcut with no registry configuration
- package inspection rejects development artifacts and pre-populated runtime state
- smoke validation checks registry creation of every production type, multiple instances, per-widget HWNDs, passive hit testing, management layout/commands, restart restore, and the single-instance guard
- idle reporting records CPU, memory, threads, handles, and TCP/UDP endpoints when query access is available

## Validation status

Pure-logic coverage is present for registry lifecycle, ID generation, single/additive/primary selection, generic locking, monitor-isolated hit testing, topology reconciliation, grid placement, authored layout selection and fitting, scene serialization and restore, atomic persistence, alignment/distribution, Clock scheduling, Calendar generation, Photo fit/fill/focal math, and asset import.

Persistence additionally migrates schema version 0, preserves unknown future widget-state keys, and copies legacy scene/photo data into the current directory hierarchy without deleting the originals. Music transport rendering now uses paired triangle geometry and rounded pause bars rather than font glyphs.

The current per-widget-window milestone compiles and links warning-clean with MSVC 19.51.36256. Clean CMake/Ninja Debug and Release configure/builds succeeded under `C:\WidgetStudioBuild`, both current CTests passed 1/1, and package creation succeeded. The current-source Debug UI smoke exercised every production widget type, registry-driven creation, multiple instances, passive margins, safe Grid/Free conversion, universal and widget-specific settings, state-preserving duplicate/remove, Lock All, single-instance enforcement, and restart restoration. Explorer exposed no usable WorkerW in that session, so the safe top-level fallback was exercised.

Rendering factories and common text formats are now shared once per process instead of recreated for every HWND. On the same nine-widget validation shape, this reduced threads from 123 to 24, handles from 1,135 to 438, working set from about 194 MB to 119 MB, and private memory from about 352 MB to 193 MB. The 30-second post-change sample averaged 0.0293% CPU and owned zero TCP/UDP endpoints.

The Visual Studio CMake/Ninja tools are usable; they are no longer a prerequisite or blocker. Windows Application Control policy `{0283ac0f-fff1-49ae-ada1-8a933130cad6}` permits the current clean Release logic test but rejects the unsigned Release GUI application at the Enterprise signing level in both its build-tree and packaged locations. The policy has not been weakened or bypassed. An organization-approved signing or allow solution is required for current Release runtime evidence. Interactive Windows 11 desktop attachment, live media transport, Photo import, startup shortcut, Explorer restart, wallpaper change, and mixed-DPI multi-monitor behavior also remain final manual gates.

Open-source publication preparation now includes an MIT license, contribution and security guidance, a documented code signing/privacy policy, and a GitHub-hosted Windows workflow for Debug/Release builds, both CTests, and a verified portable ZIP. Its SignPath submission path is manual, tag-only, and disabled until real repository variables and one scoped secret are configured after Foundation approval. Local publication validation rebuilt both configurations, passed both CTests, and verified the unsigned portable ZIP; the GitHub and SignPath portions remain unexecuted until the repository is public.
