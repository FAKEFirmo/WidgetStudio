# WidgetStudio

WidgetStudio is a local-first Windows 11 desktop widget system built with C++20, Win32, Direct2D, DirectWrite, WIC, and C++/WinRT for Windows media sessions. It has no browser runtime, cloud service, telemetry, updater, or runtime network dependency.

## Features

- Registry-driven Clock, Calendar, Photo, Music, and diagnostic widgets with multiple independent instances.
- Square 12 x 7 grid placement plus free DIP layout, alignment, matching, and distribution.
- Click/Shift-click selection, dragging, locking, duplication, removal, passive mode, and explicit widget action hit regions.
- Native Widget Library and Widget Studio windows operating on the shared live scene.
- Versioned atomic JSON persistence with backup and application-owned photo imports.
- Per-Monitor DPI Awareness V2, simultaneous monitor-scoped WorkerW surfaces, monitor association/isolation, and missing-monitor migration.
- A normal-window desktop backend by default and an explicitly opt-in experimental WorkerW backend with fallback and Explorer restart recovery.
- Event-driven invalidation. No continuous idle render loop is used.

## Safe build options

The preferred build path is the disposable Windows Sandbox workflow in [tools/dev-env/README.md](tools/dev-env/README.md). It installs the compiler and SDK only inside the temporary sandbox and writes results to `out\sandbox`.

The scripts do not enable Windows Sandbox, install host tools, change PATH, or request administrator access. On a machine where Sandbox is already available:

1. Double-click `tools\dev-env\WidgetStudio.wsb`.
2. Wait for the sandbox PowerShell task to finish.
3. Review `out\sandbox\bootstrap.log`.
4. Find the portable release under `out\sandbox\dist\WidgetStudio`.

If an MSVC x64 developer environment and CMake 3.24+ already exist on the host, the equivalent manual commands are:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release
.\tools\package.ps1 -BuildPath build -OutputPath dist
```

No tool should be installed system-wide solely to run those commands without the machine owner’s explicit approval.

## Run and controls

Run `WidgetStudio.exe`. The normal windowed backend is the supported fallback. For an isolated experimental launch attached behind desktop icons:

```powershell
$env:WIDGETSTUDIO_DESKTOP_BACKEND = 'workerw'
.\WidgetStudio.exe
```

WorkerW is undocumented Explorer behavior; attachment failure automatically falls back to the normal window.

WidgetStudio starts in passive mode. Use the tray menu or `Ctrl+Alt+W` to enter Edit Mode.

- Click selects; Shift-click toggles multi-selection.
- Drag moves unlocked widgets in the active layout mode.
- `Ctrl+Alt+W` toggles Edit Mode; `Esc` exits it.
- `Delete`, `Ctrl+D`, and `Ctrl+L` remove, duplicate, and toggle lock.
- Double-click the tray icon toggles Edit Mode.
- The tray menu opens the Widget Library and Widget Studio.
- In passive mode, only explicit widget controls accept input.

## Runtime data and removal

Normal mode writes scene data and imported assets under `%LOCALAPPDATA%\WidgetStudio`. Saves use a temporary file, flush it, replace `scene.json`, and retain `scene.json.bak`.

Imported photos are persisted as `asset://` references relative to the active data directory, so moving a complete portable release folder does not invalidate them. Existing absolute paths remain readable for compatibility.

When `portable.mode` exists beside the executable—as it does in the packaged release—data is written to `portable-data` beside the executable. Delete the release folder to remove both the portable app and its data. WidgetStudio does not create services, startup entries, scheduled tasks, registry settings, or machine-wide environment variables.

## Project documentation

- [AGENTS.md](AGENTS.md) contains mandatory implementation constraints.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) describes subsystem boundaries.
- [docs/MILESTONES.md](docs/MILESTONES.md) records implementation and validation status.
