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

## Native Windows development

Development uses CLion, MSVC Build Tools, the Windows SDK, and CLion's bundled CMake/Ninja. Build trees and generated artifacts must remain outside the source repository, under `C:\WidgetStudioBuild` by default. Windows Sandbox, Docker, WSL, VMs, containers, global package managers, and globally installed third-party libraries are not part of the workflow.

After configuring those normal native tools, run the complete command-line validation from PowerShell:

```powershell
.\tools\dev-env\validate.ps1
```

The script imports the MSVC x64 environment only for its own process, uses Ninja, runs logic tests, builds and packages Release, then performs GUI smoke and idle-resource checks. See [tools/dev-env/README.md](tools/dev-env/README.md) for CLion profile paths and individual commands.

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
- The tray menu can opt into launch-at-login using one removable per-user Startup-folder shortcut.
- In passive mode, only explicit widget controls accept input.

## Runtime data and removal

Normal mode writes scene data and imported assets under `%LOCALAPPDATA%\WidgetStudio`. Saves use a temporary file, flush it, replace `scene.json`, and retain `scene.json.bak`.

Imported photos are persisted as `asset://` references relative to the active data directory, so moving a complete portable release folder does not invalidate them. Existing absolute paths remain readable for compatibility.

When `portable.mode` exists beside the executable—as it does in the packaged release—data is written to `portable-data` beside the executable. WidgetStudio installs no service, driver, updater, scheduled task, registry setting, machine-wide environment variable, or external widget host.

Launch-at-login is disabled by default. If enabled, WidgetStudio creates only `%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\WidgetStudio.lnk`; uncheck the same tray item to remove it. To uninstall cleanly, disable that option if used, exit WidgetStudio, and delete its folder.

## Project documentation

- [AGENTS.md](AGENTS.md) contains mandatory implementation constraints.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) describes subsystem boundaries.
- [docs/MILESTONES.md](docs/MILESTONES.md) records implementation and validation status.
