# Architecture snapshot

## Core invariant

A widget owns content rendering, widget-specific settings metadata, and widget-specific state. It does **not** own desktop positioning, monitor association, selection, dragging, grid math, global appearance, DPI policy, or persistence encoding.

```text
Application
  WidgetRegistry
    WidgetDescriptor -> IWidget factory
  DesktopHost
    hidden controller HWND (tray, timers, commands)
    WidgetWindow per WidgetInstance
      Renderer
      DesktopBackendController
    WidgetLibraryWindow (enumerates registry)
    WidgetStudioWindow (shared-scene preview and settings)
    WidgetScene
      WidgetInstance -> owned IWidget content
    GridLayout
    Interaction state
  TrayController
  DesktopBackendController -> IDesktopBackend
  MonitorTopology
```

Widget registration is explicit during application startup. There is no static self-registration and no external DLL/plugin loading. Stable string type IDs are the serialization identity of built-in widget types; the host and interaction layers never branch on those IDs.

A per-user named mutex rejects a second launch before COM, rendering, or persistence initialization, ensuring every widget window and instance remains inside one process and preventing concurrent writes to the same scene.

## Widget type and instance

`WidgetDescriptor` describes a type: stable type ID, display metadata, default/minimum/maximum grid footprint, capability flags, and its content factory. Capability metadata covers widget-specific configuration, content scaling, interaction, resizing, duplication, and passive click-through. Scene and management operations consult those flags rather than concrete type IDs. `WidgetRegistry` owns the descriptor catalog, supports enumeration and lookup, and constructs `IWidget` content without depending on UI code.

`WidgetInstance` describes one scene object: unique instance ID, type ID, monitor ID, grid/free layout state, universal appearance, lock state, content scale, selection state, and an owned `IWidget`. Multiple instances can reference the same type descriptor while owning independent widget-specific state.

`IWidget` is intentionally narrow. It renders content and supplies declarative widget-specific settings and state import/export. Universal settings remain on the scene-owned instance.

## Widget lifecycle

```text
register -> discover -> create -> place -> interact -> configure
         -> serialize -> restore -> destroy
```

- Register: application startup explicitly adds compiled-in descriptors to `WidgetRegistry`.
- Discover: the Widget Library enumerates descriptors; adding a type requires no library button changes.
- Create: `WidgetScene::CreateWidget(typeId)` resolves the descriptor, calls its factory, generates an instance ID, and initializes universal defaults.
- First run: the application composition root supplies the initial built-in type ID. `DesktopHost` applies that policy generically and contains no concrete widget type IDs.
- Place: the scene scans the monitor grid left-to-right and then top-to-bottom for the first non-overlapping default footprint. A full grid falls back to a valid clamped placement.
- Interact: selection, dragging, duplication, locking, and deletion operate only on instance data.
- Configure: universal settings live on `WidgetInstance`; widget-specific settings are described by `IWidget::Settings()`.
- Serialize: the scene produces persistence records containing stable IDs, both layout modes, appearance, locking, scale, and widget state. `SceneJsonCodec` writes the versioned schema.
- Restore: the registry recreates content by type ID and applies widget state to the restored instance.
- Destroy: removing an instance destroys its owned content without type-specific cleanup in the scene.

## Layout model

Grid-mode geometry persists `column`, `row`, `columnSpan`, and `rowSpan`. Free mode reserves DIP-space `x`, `y`, `width`, and `height`. Grid cells are square and widget edges use the same shared metrics.

The initial placement scan is scene/layout policy, never widget behavior. It compares only grid instances associated with the target monitor.

## Interaction states

```text
Passive
Editing
Dragging
WidgetAction
```

In Editing mode, click establishes the primary selection, Shift-click toggles multi-selection, and unlocked instances drag through the active outer-layout mode. Grid instances snap to cells; free instances move in DIPs and clamp within the client/monitor bounds. `Delete` removes selected instances, `Ctrl+D` duplicates the primary instance, and `Ctrl+L` toggles its lock. Locked widgets remain selectable.

`IWidget::HitTestAction` and `InvokeAction` form the generic content-control boundary. The host asks the topmost widget for an explicit action before considering drag initiation, so a future Music transport button can never start a drag. In passive mode, `WM_NCHITTEST` returns `HTTRANSPARENT` except over one of these explicit actions.

## Free layout and alignment

`OuterLayout` resolves grid or free instance rectangles and moves free instances without mixing physical pixels and DIPs. `Alignment` implements primary-relative left/center/right/top/middle/bottom alignment, width/height/both matching, and equal horizontal/vertical distribution. Locked items are not modified. Scene APIs expose mode conversion and selected-item alignment without widget-type knowledge.

Free-layout duplication first tries adjacent positions separated by 24 DIPs, then scans a deterministic 24-DIP lattice for a non-overlapping in-bounds position. If the surface is completely full, the fallback remains bounded and deterministic. Grid duplication continues to use the shared first-free grid scan.

## Persistence boundary

`WidgetPersistenceRecord` is the encoding-neutral persistence model. It contains instance/type/monitor IDs, grid and free placements, layout mode, lock state, content scale, universal appearance, and widget-specific key/value state.

`SceneJsonCodec` encodes schema version 1 without a third-party dependency and rejects malformed input, unsupported future versions, invalid values, and duplicate instance IDs. Schema version 0 migrates missing monitor associations to the primary display. Unknown JSON fields are ignored safely. The scene preserves unknown widget-state keys when a known older widget restores and re-saves a newer record. Records whose widget type is unavailable are retained and written back instead of being discarded.

`SceneStore` writes a temporary file in the configuration directory, flushes it, and uses same-volume Win32 replacement. When replacing an existing configuration it retains the previous file as `scene.json.bak`. Normal data lives under `%LOCALAPPDATA%\WidgetStudio`; an explicit `portable.mode` sentinel redirects configuration, imported images, and cache data to `data\config`, `data\images`, and `data\cache` beside the executable. A legacy portable scene is copied forward without deleting the old file.

## Authored content layout model

`AuthoredContentLayout` owns profile selection, optional breakpoint hysteresis, uniform fitting, and explicit centering. Given available content `(Wa, Ha)` and reference dimensions `(Wr, Hr)`, it uses one base scale `s = min(Wa / Wr, Ha / Hr)` and centers the resulting rectangle. Universal content scaling is applied to the fitted composition as a whole. Individual components are never stretched independently.

Widgets draw reference geometry through one Direct2D transform. Clock uses a 270 x 120 reference composition. The established Music profiles are represented as ordinary authored-layout profiles rather than host conditionals.

## Event-driven content updates

`IWidget::NextUpdateTime()` optionally advertises the next instant at which rendered content can change. `DesktopHost` scans instances and arms one Win32 one-shot timer for the earliest request. The Clock requests the next minute boundary by default or the next second boundary when seconds are enabled. There is no continuous render loop.

Music metadata, playback state, timeline state, and artwork originate only from `GlobalSystemMediaTransportControlsSessionManager` through the shared `windows/MediaSessionService`. Session events update a mutex-protected snapshot and post a lightweight host notification. Artwork bytes are shared and refreshed only on media-property changes; playback/timeline events do not re-fetch or copy artwork. While playing, Music requests a 500 ms one-shot progress refresh.

All WinRT media-session calls run on a dedicated MTA worker. The service initializes lazily when the first Music instance is constructed, so scenes without Music do not pay for a media thread or session manager. Once active, the worker blocks on a condition variable when idle, wakes for session events or transport commands, and publishes immutable snapshots back to the UI. This avoids blocking WinRT waits on the application STA and gives shutdown a single resource-owning thread.

Music uses the established Portrait, Square, Landscape, and Ultra-wide authored profiles. Each profile contains exactly one square artwork region, one progress bar, elapsed/negative-remaining labels, and Previous/Play-Pause/Next vector controls. Transport hit regions are returned through the generic widget-action interface.

## Wallpaper and glass

`WallpaperCache` decodes the current wallpaper once per process into a shared WIC bitmap. Each HWND renderer creates only the small device-dependent region needed for that widget; Widget Studio creates a monitor-sized preview region only while its window is open. A widget window samples the monitor-relative wallpaper area behind its scene rectangle, so the rounded card composes consistently without screen capture. Glass-enabled cards cache a further downsampled regional bitmap and bilinearly expand it beneath a rounded geometry mask. Cache entries invalidate only when the wallpaper, render target, DPI, card geometry, or blur radius changes. No continuous capture or blur loop is used.

`RenderingResources` owns one process-wide Direct2D factory, shared DirectWrite factory, WIC factory, and common text formats. Per-widget `Renderer` objects retain only their HWND-specific render targets and device-dependent bitmap caches. This preserves independent lightweight windows while avoiding a full graphics-factory stack for every widget.

## Photo invariant

Source-image aspect ratio is immutable. `PhotoLayout` provides proportional `fill` (uniform scale plus focal crop) and `fit` (uniform scale plus letterbox/pillarbox) calculations as pure tested logic. User content scale uniformly sizes the centered image composition and clips zoomed content to the card content area. `PhotoWidget` decodes local files with WIC and keeps a separate bitmap cache for each active render target and resource generation. Music artwork follows the same rule. This is required because the live widget instance renders into both its desktop HWND and the Studio preview, while Direct2D bitmaps are target-dependent resources. `AssetLibrary` copies chosen source files into application-owned local storage through a temporary file and same-volume move.

New imports persist as validated `asset://filename` references. The composition root supplies the active image directory to the Photo factory, allowing portable releases to move as a unit without coupling the widget to global path discovery. Legacy absolute image paths remain supported. The data-path migration copies legacy scene and imported-image files forward without deleting their originals.

## Calendar model

`CalendarModel` is widget-domain logic independent of rendering. It produces a fixed six-week grid with adjacent-month cells, today/weekend flags, Gregorian leap-year handling, and Monday/Sunday start. `CalendarWidget` localizes month, year, and weekday labels through Windows and requests its next content update at local midnight.

## Widget Studio

`WidgetStudioWindow` renders the same `WidgetScene` through the same `Renderer`, `GridLayout`, and authored widget content used by the desktop. Its preview preserves the selected monitor work-area aspect ratio, and wallpaper cropping stays anchored to the full monitor so a taskbar offset cannot shift the preview relative to the desktop card. A preview-only render transform fits the active monitor scene without maintaining duplicate layout state. Universal instance fields and declarative `IWidget::Settings()` definitions drive native controls for numbers, booleans, choices, text, and local files. Universal appearance includes theme, frosted/transparent/solid surface mode, opacity, blur, radius, padding, border, and shadow. Changes apply to the live scene and are persisted through the normal scene-change callback. A topology-backed monitor selector moves selected instances generically and switches the preview to the destination work area.

## Desktop and monitor boundary

`IDesktopBackend` isolates attachment behavior from the scene, renderer, interaction system, and widgets. Every `WidgetWindow` owns a backend controller. It attempts WorkerW attachment by default and falls back to a non-activating bottom-z-order popup if Explorer's undocumented desktop host cannot be discovered. `WIDGETSTUDIO_DESKTOP_BACKEND=windowed` forces the fallback for diagnostics. Attachment failure never affects scene state.

`DesktopHost` is a hidden controller HWND for tray callbacks, global commands, event-driven timers, and subsystem lifetime. It synchronizes exactly one lightweight `WidgetWindow` with every live scene instance. Each widget window computes its own physical position from monitor-local DIPs and effective DPI, renders only its instance, and performs only that widget's action hit testing. Passive regions return `HTTRANSPARENT`; Edit Mode temporarily makes the surface selectable and draggable. `WidgetWindowPlacementCalculator` keeps DIP-to-pixel conversion testable outside Win32 interaction code.

`MonitorTopology` enumerates stable display device IDs, effective DPI, full-monitor pixels, and work-area pixels/DIPs. Widget placement is relative to the work area so it avoids taskbars, while wallpaper sampling remains anchored to the full monitor even with a top or left taskbar. The last interacted widget window becomes the Widget Studio and Widget Library target. Display changes reposition all widget HWNDs; every instance is reconciled with the current grid and work area, while instances whose saved display disappeared first migrate to the primary monitor. Any corrected geometry is saved. Per-widget `WM_DPICHANGED` notifications coalesce into one topology refresh. After Explorer broadcasts `TaskbarCreated`, surviving windows reattach and destroyed child windows are recreated from the scene.

Coordinate spaces are explicit at the native boundary: `MonitorTopology` retains work-area and full-monitor rectangles in physical screen pixels and exposes a monitor-local work area in DIPs. Scene grid/free placement and drag offsets are always monitor-local DIPs. `WidgetWindowPlacementCalculator` is the only scene-to-screen placement conversion and adds the work-area physical origin after scaling DIPs by the monitor DPI. Mouse messages begin as client pixels, are converted with the widget HWND DPI, then add the monitor-local window origin before entering layout/drag code. Direct2D targets use the HWND DPI and therefore consume DIPs. Wallpaper sampling separately converts physical offsets from the full-monitor origin back to DIPs; it never reuses work-area-relative scene coordinates as screen pixels.

`D2DERR_RECREATE_TARGET` discards every HWND-specific target and bitmap cache. The next invalidated paint recreates them from process-shared factories; resize never retains a rejected target. Rendering is driven only by paint invalidation, system/media events, and one-shot content timers.

## Delivery boundary

The Release target statically links the MSVC runtime and installs `WidgetStudio.exe`, a runtime README, the `portable.mode` marker, and the empty application-owned `assets`/`data` directory structure. No compiler, SDK, service, registry entry, updater, or runtime package manager is part of the application release. DebugWidget is registered and linked only in Debug configurations.

WidgetStudio is the sole widget host and keeps every widget HWND, integration, and management window in its single guarded process. Portable mode redirects runtime state and imported assets to `data` within the release folder. Optional launch-at-login uses one per-user `WidgetStudio.lnk` in the Windows Startup folder, managed from the tray and removable without registry access. Normal removal is: disable launch-at-login if enabled, exit the process, and delete the release folder.

Development builds are equally isolated from the repository: CLion or the native PowerShell workflow generates Ninja trees under `C:\WidgetStudioBuild`. No VM, container, subsystem, package manager, or second runtime environment participates in building or running the application.
