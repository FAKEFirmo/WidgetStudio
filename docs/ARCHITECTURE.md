# Architecture snapshot

## Core invariant

A widget owns content and widget-specific state. It does **not** own desktop positioning, selection, dragging, grid math, DPI policy, or persistence format.

```text
Application
  DesktopHost
    WidgetScene
    GridLayout
    Renderer
    Interaction state
  TrayController
```

## Layout model

Grid-mode widget geometry is persisted as:

```text
column
row
columnSpan
rowSpan
```

The grid computes square cells from the monitor/client rectangle. Widget outer edges therefore share identical cell boundaries and gaps.

Free/Align mode will later persist DIP-space `x/y/width/height` independently of grid mode.

## Interaction states

```text
Passive
Editing
Dragging
WidgetAction (future, e.g. music buttons)
```

In Editing mode:

- click = primary selection
- Shift-click = multi-selection
- primary selection uses the stronger selection outline
- drag = grid-snapped movement
- locked widgets remain selectable but cannot move

In the future real desktop host, passive widget regions will return `HTTRANSPARENT`; explicit control hit regions such as Music Play/Pause will remain interactive.

## Authored content layout model

Complex widgets such as Music will expose discrete reference compositions. A reference layout has:

```text
referenceWidth
referenceHeight
ratio range
minimum scale
component geometry
```

Given available content space `(Wa, Ha)` and reference dimensions `(Wr, Hr)`:

```text
s = min(Wa / Wr, Ha / Hr)
renderedW = Wr * s
renderedH = Hr * s
x = (Wa - renderedW) / 2
y = (Ha - renderedH) / 2
```

The whole reference composition is scaled once by `s`. Individual artwork, vector controls and typography are never stretched independently.

## Photo invariant

Source-image aspect ratio is immutable. Rendering supports only proportional `cover` (uniform scale + crop) and `contain` (uniform scale + letterbox/pillarbox).
