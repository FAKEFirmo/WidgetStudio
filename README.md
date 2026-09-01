# WidgetStudio

WidgetStudio is a local-first Windows 11 desktop widget system built with C++20, Win32, Direct2D, DirectWrite, WIC, and C++/WinRT for Windows media sessions. It has no browser runtime, cloud service, telemetry, updater, or runtime network dependency.

## Features

- Registry-driven Clock, Calendar, Photo, and Music widgets with multiple independent instances. A diagnostic widget is available only in Debug builds.
- Square 12 x 7 grid placement plus free DIP layout, alignment, matching, and distribution.
- Click/Shift-click selection, dragging, locking, duplication, removal, passive mode, and explicit widget action hit regions.
- Native Widget Library and Widget Studio windows operating on the shared live scene.
- Generic capability-aware settings, including monitor assignment, and a shared-scene preview.
- Versioned/migrating atomic JSON persistence with backup, unknown-setting preservation, and application-owned photo imports.
- Per-Monitor DPI Awareness V2, one lightweight HWND per widget, monitor association/isolation, missing-monitor migration, and work-area/resolution reconciliation.
- Real-desktop WorkerW attachment by default, with a non-crashing normal-window fallback and Explorer restart recovery.
- Event-driven invalidation. No continuous idle render loop is used.

## Native Windows development

Development uses CLion, MSVC Build Tools, the Windows SDK, and CLion's bundled CMake/Ninja. Build trees and generated artifacts must remain outside the source repository, under `C:\WidgetStudioBuild` by default. Windows Sandbox, Docker, WSL, VMs, containers, global package managers, and globally installed third-party libraries are not part of the workflow.

After configuring those normal native tools, run the complete command-line validation from PowerShell:

```powershell
.\tools\dev-env\validate.ps1
```

The script imports the MSVC x64 environment only for its own process, uses Ninja, runs logic tests, builds and packages Release, then performs GUI smoke and idle-resource checks. See [tools/dev-env/README.md](tools/dev-env/README.md) for CLion profile paths and individual commands.

GitHub Actions repeats clean Debug and Release builds and CTest runs on a GitHub-hosted Windows runner. It packages the Release tree as `WidgetStudio-portable.zip`; all build trees remain under the runner's temporary directory rather than the source checkout. Trusted signing is deliberately disabled until the public repository has been approved and configured by SignPath Foundation. See [docs/PUBLISHING.md](docs/PUBLISHING.md).

## Run and controls

Run `WidgetStudio.exe`. Widget windows attempt Explorer desktop attachment automatically. WorkerW is undocumented Explorer behavior, so attachment failure falls back safely to non-activating bottom-z-order windows. To force that fallback for diagnostics:

```powershell
$env:WIDGETSTUDIO_DESKTOP_BACKEND = 'windowed'
.\WidgetStudio.exe
```

All widget HWNDs, management windows, tray handling, media integration, and persistence remain in one `WidgetStudio.exe` process.

WidgetStudio starts in passive mode. Use the tray menu or `Ctrl+Alt+W` to enter Edit Mode.

- Click selects; Shift-click toggles multi-selection.
- Drag moves unlocked widgets in the active layout mode.
- `Ctrl+Alt+W` toggles Edit Mode; `Esc` exits it.
- `Delete`, `Ctrl+D`, and `Ctrl+L` remove, duplicate, and toggle lock.
- Double-click the tray icon toggles Edit Mode.
- The tray menu opens the Widget Library and Widget Studio.
- Widget Studio can move selected widgets between discovered monitors and clamps free geometry to the destination work area.
- The tray menu can opt into launch-at-login using one removable per-user Startup-folder shortcut.
- In passive mode, only explicit widget controls accept input.

## Runtime data and removal

Normal mode writes scene data under `%LOCALAPPDATA%\WidgetStudio\config`, imported images under `%LOCALAPPDATA%\WidgetStudio\images`, and disposable cached data under `%LOCALAPPDATA%\WidgetStudio\cache`. Saves use a temporary file, flush it, replace `scene.json`, and retain `scene.json.bak`.

Imported photos are persisted as `asset://` references relative to the active data directory, so moving a complete portable release folder does not invalidate them. Existing absolute paths remain readable for compatibility.

When `portable.mode` exists beside the executable—as it does in the packaged release—state stays under `data\config`, `data\images`, and `data\cache` beside the executable. Existing `portable-data\scene.json` and `portable-data\assets` files are copied into the new locations once for compatibility. WidgetStudio installs no service, driver, updater, scheduled task, registry setting, machine-wide environment variable, or external widget host.

During the current startup-diagnostic phase, every launch writes a deterministic UTF-8 trace to `data\logs\startup.log` in portable mode, or `%LOCALAPPDATA%\WidgetStudio\logs\startup.log` in normal mode. The trace records the Windows build, single-instance decision, DPI/COM initialization, registry, host window, tray, monitors, widget windows, and message-loop entry or exact failure code. It contains no telemetry and is never transmitted.

Launch-at-login is disabled by default. If enabled, WidgetStudio creates only `%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\WidgetStudio.lnk`; uncheck the same tray item to remove it. To uninstall cleanly, disable that option if used, exit WidgetStudio, and delete its folder.

## Project documentation

- [AGENTS.md](AGENTS.md) contains mandatory implementation constraints.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) describes subsystem boundaries.
- [docs/MILESTONES.md](docs/MILESTONES.md) records implementation and validation status.
- [docs/ACCEPTANCE.md](docs/ACCEPTANCE.md) separates implemented behavior from remaining final runtime gates.
- [docs/RELEASE.md](docs/RELEASE.md) is the build, operation, data, removal, API, and platform guide.

## Code signing policy

Free code signing provided by [SignPath.io](https://signpath.io/), certificate by [SignPath Foundation](https://signpath.org/). The CI workflow can submit only the exact portable archive built and tested on GitHub-hosted Windows runners, and every Release signing request requires approval. Team roles, privacy commitments, and release verification are documented in [docs/CODE_SIGNING_POLICY.md](docs/CODE_SIGNING_POLICY.md).

## License and contributions

WidgetStudio is available under the [MIT License](LICENSE). Contributions are welcome under [CONTRIBUTING.md](CONTRIBUTING.md); security reports should follow [SECURITY.md](SECURITY.md).
