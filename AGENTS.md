# WidgetStudio Agent Instructions

## Product goal
Build a native Windows 11 desktop widget system with a consistent visual language, very low idle resource usage, local-first behavior, and a shared layout/interaction engine. The initial widget set is Clock, Calendar, Music, and Photo.

## Required technology
- C++20.
- Win32 for the application/window layer.
- Direct2D for 2D rendering.
- DirectWrite for text.
- WIC for image decoding.
- C++/WinRT only where Windows Runtime APIs are actually required, such as Global System Media Transport Controls.
- CMake as the only project/build definition. Keep the project directly openable in CLion.
- Target Windows 11 x64 first.

Do not introduce Electron, WebView, Qt, Rainmeter, .NET, JavaScript runtimes, browser rendering, background web services, or a second UI framework unless the user explicitly changes the architecture.

## Runtime constraints
- Prefer event-driven updates over polling.
- Never run a continuous 60 FPS render loop while the desktop is idle.
- Keep network access disabled by default. Widgets must not contact web APIs unless a future widget explicitly requires an opt-in network capability.
- Keep one application process and shared rendering/layout infrastructure.
- Treat undocumented Explorer/WorkerW attachment as an optional backend, not a dependency of widget code.

## Core architecture
Maintain these boundaries:
- `app/`: process lifetime, tray controller, top-level commands.
- `desktop/`: host window and desktop attachment backends.
- `scene/`: widget instances, z-order, selection, scene state.
- `layout/`: grid, free layout, alignment, authored content layout math.
- `rendering/`: Direct2D/DirectWrite/WIC rendering primitives.
- `interaction/`: edit mode, hit testing, dragging, widget actions.
- `widgets/`: widget-specific content/data only.
- `windows/`: Windows-specific integrations such as media sessions, wallpaper, DPI and monitors.
- `persistence/`: local configuration and imported assets.

Do not let an individual widget implement desktop positioning, selection, grid snapping, or general drag behavior.

Widget types are registered explicitly at application startup using stable string type IDs. A widget type descriptor/factory and a scene-owned widget instance are separate concepts. Desktop host, scene, renderer, and interaction code must remain free of concrete widget-type conditionals.

## Layout rules
Two outer-layout modes are planned:
1. Grid mode stores `column`, `row`, `columnSpan`, `rowSpan`.
2. Free mode stores `x`, `y`, `width`, `height` in DPI-independent units.

Grid cells must be square. Widget edges and gaps must resolve from the same grid metrics so aligned widgets never have near-miss borders.

Support single selection and Shift-click multi-selection. The primary selection is the reference for matching/alignment operations.

There are no desktop resize handles. Resize is intentional through settings/grid spans/free size controls.

## Authored content layouts
Complex widgets such as Music use discrete authored reference compositions selected by outer-card aspect ratio. Do not independently stretch internal components.

For a reference composition `Wr x Hr` inside available content space `Wa x Ha`, use one uniform scale:

`s = min(Wa / Wr, Ha / Hr)`

Then center the rendered reference rectangle explicitly:

`x = (Wa - Wr*s) / 2`
`y = (Ha - Hr*s) / 2`

All reference geometry, typography, spacing and icons scale by the same factor. Preserve intrinsic media proportions. Album artwork is always 1:1.

## Widget visual system
Default reference values:
- Corner radius: about 22-24 DIP.
- Inner padding: about 18-22 DIP.
- Dark glass opacity: about 0.60-0.65.
- Blur reference: about 18 DIP.
- Border: subtle 1 DIP low-contrast edge.
- Typography: Segoe UI Variable by default.
- Icons: vector geometry; icon shapes must not depend on the selected text font.

The actual Windows wallpaper is part of the composition and should be used in Widget Studio/desktop previews.

## Initial widgets
### Clock
Large time, divider, date. Update only when the displayed value changes.

### Calendar
Month/year header, seven-column calendar, today highlight, dim adjacent-month days. No internet dependency.

### Music
Exactly one progress bar plus previous, play/pause and next controls in the base design. Use Windows media-session APIs rather than service-specific APIs. Modern controls are bold rounded vector glyphs. Implement authored ratio-specific layouts and uniform fitting.

### Photo
Local images only by default. Preserve original image aspect ratio. Support cover/crop and contain. Focal position may later become normalized `0..1` coordinates. Imported assets should eventually be copied into the local WidgetStudio asset library so source-file moves do not break widgets.

## Interaction model
Normal desktop mode:
- Passive areas are click-through.
- Only explicit widget hit regions receive input, e.g. music controls.

Edit mode:
- Widget surfaces can be selected.
- Shift-click multi-selects.
- Dragging moves unlocked widgets.
- Locked widgets remain selectable/configurable but cannot move.
- Escape exits edit mode.

Keep widget control hit testing separate from drag initiation so pressing Play can never start a drag.

## Code quality
- Prefer small focused classes over large managers.
- Use RAII for handles/resources where practical.
- Treat HRESULT/Win32 failures explicitly.
- Keep headers lean and avoid unnecessary Windows headers in common/domain-only files.
- Enable warnings and keep the build warning-clean.
- Avoid hidden global mutable state.
- Add comments for non-obvious Windows behavior, not for straightforward C++.

## Build and validation
CLion is the primary IDE. Use its Visual Studio/MSVC toolchain on Windows and CMake profiles.

The command-line validation path is:

`cmake -S . -B build -G "Visual Studio 17 2022" -A x64`
`cmake --build build --config Debug`

Before declaring a coding task complete:
1. Configure successfully.
2. Build Debug successfully.
3. Fix new warnings when practical.
4. Run the app for UI/runtime changes when the environment supports Windows GUI execution.
5. Summarize changed files, behavior, remaining risks, and the next recommended step.

## Development workflow
Work in small milestones. Do not implement several large subsystems in one task unless asked. Preserve the architecture documents when changing a fundamental decision.

Current priority order:
1. Shared scene/layout/interaction foundation.
2. Widget framework and registry-driven library.
3. Authored content-layout engine.
4. Production Clock widget.
5. Widget Studio settings window backed by the same scene.
6. Persistence.
7. Calendar.
8. Photo.
9. Music/media sessions.
10. Multi-monitor/DPI hardening.
11. Desktop attachment backend and final performance/polish.
