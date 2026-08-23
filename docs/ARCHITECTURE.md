# Architecture snapshot

## Core invariant

A widget owns content rendering, widget-specific settings metadata, and widget-specific state. It does **not** own desktop positioning, monitor association, selection, dragging, grid math, global appearance, DPI policy, or persistence encoding.

```text
Application
  WidgetRegistry
    WidgetDescriptor -> IWidget factory
  DesktopHost
    WidgetLibraryWindow (enumerates registry)
    WidgetScene
      WidgetInstance -> owned IWidget content
    GridLayout
    Renderer
    Interaction state
  TrayController
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
- Serialize: the scene produces persistence records containing stable IDs, both layout modes, appearance, locking, scale, and widget state. File encoding is deferred.
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
WidgetAction (future, e.g. music buttons)
```

In Editing mode, click establishes the primary selection, Shift-click toggles multi-selection, and unlocked instances drag with grid snapping. `Delete` removes selected instances, `Ctrl+D` duplicates the primary instance, and `Ctrl+L` toggles its lock. Locked widgets remain selectable.

In the future desktop backend, passive widget regions will return `HTTRANSPARENT`; explicit content-control regions will remain interactive and separate from drag initiation.

## Persistence boundary

`WidgetPersistenceRecord` is the encoding-neutral persistence model. It contains instance/type/monitor IDs, grid and free placements, layout mode, lock state, content scale, universal appearance, and widget-specific key/value state. A later milestone may encode these records under `%LOCALAPPDATA%` without changing widget or host APIs.

## Authored content layout model

Complex widgets will expose discrete reference compositions. Given available content `(Wa, Ha)` and reference dimensions `(Wr, Hr)`, use one uniform scale `s = min(Wa / Wr, Ha / Hr)` and explicitly center the resulting rectangle. Individual components are never stretched independently.

## Photo invariant

Source-image aspect ratio is immutable. Rendering supports only proportional `cover` (uniform scale plus crop) and `contain` (uniform scale plus letterbox/pillarbox).
