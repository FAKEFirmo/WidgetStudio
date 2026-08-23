# Architecture snapshot

## Core invariant

A widget owns content rendering, widget-specific settings metadata, and widget-specific state. It does **not** own desktop positioning, monitor association, selection, dragging, grid math, global appearance, DPI policy, or persistence encoding.

```text
Application
  WidgetRegistry
    WidgetDescriptor -> IWidget factory
  DesktopHost
    WidgetLibraryWindow (enumerates registry)
    WidgetStudioWindow (shared-scene preview and settings)
    WidgetScene
      WidgetInstance -> owned IWidget content
    GridLayout
    Renderer
    Interaction state
  TrayController
  DesktopBackendController -> IDesktopBackend
  MonitorTopology
```

Widget registration is explicit during application startup. There is no static self-registration and no external DLL/plugin loading. Stable string type IDs are the serialization identity of built-in widget types; the host and interaction layers never branch on those IDs.

## Widget type and instance

`WidgetDescriptor` describes a type: stable type ID, display metadata, default/minimum/maximum grid footprint, capability flags, and its content factory. `WidgetRegistry` owns the descriptor catalog, supports enumeration and lookup, and constructs `IWidget` content without depending on UI code.

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

## Persistence boundary

`WidgetPersistenceRecord` is the encoding-neutral persistence model. It contains instance/type/monitor IDs, grid and free placements, layout mode, lock state, content scale, universal appearance, and widget-specific key/value state.

`SceneJsonCodec` encodes schema version 1 without a third-party dependency and rejects malformed input, unsupported versions, invalid values, and duplicate instance IDs. Unknown JSON fields are ignored for forward compatibility. Records whose widget type is unavailable are retained and written back instead of being discarded.

`SceneStore` writes a temporary file in the configuration directory, flushes it, and uses same-volume Win32 replacement. When replacing an existing configuration it retains the previous file as `scene.json.bak`. Normal data lives under `%LOCALAPPDATA%\WidgetStudio`; an explicit `portable.mode` sentinel beside the executable redirects it to `portable-data` beside the executable.

## Authored content layout model

`AuthoredContentLayout` owns profile selection, optional breakpoint hysteresis, uniform fitting, and explicit centering. Given available content `(Wa, Ha)` and reference dimensions `(Wr, Hr)`, it uses one base scale `s = min(Wa / Wr, Ha / Hr)` and centers the resulting rectangle. Universal content scaling is applied to the fitted composition as a whole. Individual components are never stretched independently.

Widgets draw reference geometry through one Direct2D transform. Clock uses a 270 x 120 reference composition. The established Music profiles are represented as ordinary authored-layout profiles rather than host conditionals.

## Event-driven content updates

`IWidget::NextUpdateTime()` optionally advertises the next instant at which rendered content can change. `DesktopHost` scans instances and arms one Win32 one-shot timer for the earliest request. The Clock requests the next minute boundary by default or the next second boundary when seconds are enabled. There is no continuous render loop.

Music metadata, playback state, timeline state, and artwork originate only from `GlobalSystemMediaTransportControlsSessionManager` through the shared `windows/MediaSessionService`. Session events update a mutex-protected snapshot and post a lightweight host notification. Artwork bytes are shared and refreshed only on media-property changes; playback/timeline events do not re-fetch or copy artwork. While playing, Music requests a 500 ms one-shot progress refresh.

All WinRT media-session calls run on a dedicated MTA worker. The worker blocks on a condition variable when idle, wakes for session events or transport commands, and publishes immutable snapshots back to the UI. This avoids blocking WinRT waits on the application STA and gives shutdown a single resource-owning thread.

Music uses the established Portrait, Square, Landscape, and Ultra-wide authored profiles. Each profile contains exactly one square artwork region, one progress bar, elapsed/negative-remaining labels, and Previous/Play-Pause/Next vector controls. Transport hit regions are returned through the generic widget-action interface.

## Wallpaper and glass

`Renderer` decodes the current wallpaper once through WIC and uses the same cached bitmap for the desktop and preview. Glass-enabled cards cache a low-resolution sample of the exact wallpaper region behind the card and bilinearly expand it beneath a rounded geometry mask; the downsample factor follows the configured blur radius. Cache entries invalidate only when the wallpaper, render target, DPI, card geometry, or blur radius changes. No desktop capture or continuous blur pass is used.

## Photo invariant

Source-image aspect ratio is immutable. `PhotoLayout` provides proportional `fill` (uniform scale plus focal crop) and `fit` (uniform scale plus letterbox/pillarbox) calculations as pure tested logic. `PhotoWidget` decodes local files with WIC and caches its target-dependent Direct2D bitmap by render-resource generation. `AssetLibrary` copies chosen source files into application-owned local storage through a temporary file and same-volume move.

## Calendar model

`CalendarModel` is widget-domain logic independent of rendering. It produces a fixed six-week grid with adjacent-month cells, today/weekend flags, Gregorian leap-year handling, and Monday/Sunday start. `CalendarWidget` localizes month, year, and weekday labels through Windows and requests its next content update at local midnight.

## Widget Studio

`WidgetStudioWindow` renders the same `WidgetScene` through the same `Renderer`, `GridLayout`, and authored widget content used by the desktop. A preview-only render transform fits the active monitor scene without maintaining duplicate layout state. Universal instance fields and declarative `IWidget::Settings()` definitions drive native controls for numbers, booleans, choices, text, and local files.

## Desktop and monitor boundary

`IDesktopBackend` isolates attachment behavior from the scene, renderer, interaction system, and widgets. `DesktopBackendController` selects the reliable normal-window backend unless the process-local `WIDGETSTUDIO_DESKTOP_BACKEND=workerw` option requests the experimental backend. WorkerW discovery and attachment can fail without affecting widget state; the controller falls back to a normal window and reattaches after Explorer broadcasts `TaskbarCreated`.

`MonitorTopology` enumerates stable display device IDs, effective DPI, and DIP work areas. Rendering and hit testing filter on the owning monitor ID so instances from different monitor scenes cannot overlap logically. In experimental desktop-attached mode, one primary host surface and a lightweight `DesktopSurface` for every additional display render the same process-owned scene with per-window DPI, grid metrics, input, and renderer resources. The last interacted surface becomes the Widget Studio and Widget Library target. Display changes rebuild those surfaces; instances whose saved display disappeared migrate to the primary monitor and clamp to its grid/work area before the scene is saved.

## Delivery boundary

The Release target statically links the MSVC runtime and installs only `WidgetStudio.exe`, a runtime README, and the `portable.mode` marker. No compiler, SDK, service, registry entry, updater, or runtime package manager is part of the application release.
