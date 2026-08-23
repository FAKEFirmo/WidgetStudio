# WidgetStudio

Native Windows 11 desktop widget system built with C++20, Win32, Direct2D, DirectWrite and WIC.

## Development tools

- **CLion** is the primary IDE.
- **JetBrains Air** is used for agentic development against the same repository.
- **CMake** is the single source of truth for the build.
- **MSVC + Windows 11 SDK** are the primary Windows toolchain. You only need the Visual Studio Build Tools; the Visual Studio IDE itself is not required.

## First-time Windows setup

Install:

1. JetBrains CLion.
2. JetBrains Air.
3. Git.
4. Visual Studio 2022 Build Tools with **Desktop development with C++** and a Windows 11 SDK.
5. CMake (CLion also bundles CMake, but a command-line installation is useful for Air tasks).

### CLion

Open the `WidgetStudio` folder directly.

In **Settings | Build, Execution, Deployment | Toolchains** create/select a Visual Studio toolchain using MSVC x64. Then use a normal Debug CMake profile. CLion can use its bundled Ninja/CMake with that toolchain; no `.sln` file is required.

### JetBrains Air

Open the exact same `WidgetStudio` folder as the Air project/workspace. Initialize Git before running agent tasks:

```powershell
git init
git add .
git commit -m "Bootstrap WidgetStudio"
```

Air reads `AGENTS.md` as project instructions, so architectural constraints and validation rules live in version control with the code.

## Current milestone

The current foundation and widget framework include:

- Win32 application lifecycle
- Per-monitor DPI awareness V2
- Direct2D / DirectWrite renderer
- WIC current-wallpaper loading
- 12 x 7 square grid
- explicit widget descriptors, factories, and a central registry
- generic widget instances with stable type IDs and unique instance IDs
- a registry-driven native Widget Library
- a minimal DebugWidget used to validate multiple instances
- versioned local scene persistence with atomic replacement and backup
- click selection and Shift-click multi-selection
- grid-snapped dragging
- widget lock state
- edit/passive mode
- tray icon and edit-mode hotkey

The development host is intentionally a normal window for now. Actual Explorer desktop attachment will remain behind a backend abstraction and will be added only after the scene, rendering and interaction engine are stable.

## Command-line build

From a Developer PowerShell with the MSVC environment available:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
.\build\Debug\WidgetStudio.exe
```

## Controls in the current development host

- Click a widget: select it.
- Shift-click: add/remove it from the selection.
- Drag: move an unlocked widget on the square grid.
- `Ctrl + Alt + W`: toggle edit mode.
- `Esc`: exit edit mode.
- Double-click the tray icon: toggle edit mode.
- Right-click the tray icon: add a widget, edit/finish editing, or exit.
- `Delete`: remove selected widgets in edit mode.
- `Ctrl+D`: duplicate the primary selection.
- `Ctrl+L`: toggle the primary selection's lock.

## Runtime data

By default, scene data is written to `%LOCALAPPDATA%\WidgetStudio\scene.json`. Atomic replacement keeps the previous file at `scene.json.bak`.

For portable-data mode, place an empty file named `portable.mode` beside `WidgetStudio.exe` before launch. Scene data is then written to `portable-data\scene.json` beside the executable.

WidgetStudio does not use telemetry or network access at runtime.

## Disposable build environment

The repository contains a Windows Sandbox workflow under `tools\dev-env`. It maps source read-only, provisions Microsoft build tools inside the disposable VM, runs the build and tests, and copies artifacts to `out\sandbox`. It does not enable Windows Sandbox or install tools on the host. See [tools/dev-env/README.md](tools/dev-env/README.md).

## Project guidance

Read these before major changes:

- `AGENTS.md` — mandatory coding/architecture instructions for Air and other agents.
- `docs/ARCHITECTURE.md` — system boundaries and Windows design.
- `docs/MILESTONES.md` — implementation sequence.
