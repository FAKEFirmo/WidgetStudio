# Acceptance status

This file tracks evidence against the final product goal. “Implemented” means the current source contains the behavior; it is not a substitute for the runtime gates below.

## Implemented in the current source

- One guarded `WidgetStudio.exe` process with a hidden controller and one lightweight HWND per widget instance.
- Explicit built-in registry, registry-driven Widget Library, multiple instances, generic create/duplicate/remove/lock lifecycle, and Debug-only diagnostic widget.
- Per-monitor grid/free geometry, first-free placement, selection/Shift-selection, drag, lock, alignment/matching/distribution, descriptor size constraints, DPI conversion, z-order synchronization, and monitor reassignment.
- Per-window passive hit testing with generic interactive regions; Music exposes only Previous, Play/Pause, and Next.
- Shared authored-layout fitting and the four required Music aspect profiles.
- Clock, Calendar, Photo, and Windows media-session Music implementations with declarative settings/state.
- Versioned atomic JSON persistence, schema-0 migration, unknown-setting/type preservation, and legacy data migration.
- Portable `data\config`, `data\images`, and `data\cache` paths plus removable Startup-folder launch shortcut.
- Process-shared rendering factories/text formats and wallpaper decode, per-widget render targets/bitmap regions, event-driven per-instance redraw scheduling, and lazy media-service startup.
- Default Explorer/WorkerW attachment with a non-crashing windowed fallback and Explorer restart retry.

## Evidence collected

- Commit `08d545f` checkpoints the per-widget HWND architecture.
- MSVC 19.51 `/W4 /permissive-` no-output compilation passes for the full current C++ source and logic-test source.
- Clean CMake/Ninja Debug and Release configure/builds succeeded under `C:\WidgetStudioBuild` with MSVC 19.51.36256.
- Current Debug CTest passed 1/1 after a clean rebuild. The current Release test binary compiled and linked, but Application Control rejected that unsigned hash before process startup; the same Release tests passed 1/1 immediately before the rendering-resource refactor.
- The current portable Debug smoke passed with production widgets present. It verified one guarded process, registry enumeration, all four production widget types, multiple instances, matching per-widget HWND counts, passive `HTTRANSPARENT` margins, Widget Studio preview-above-settings layout, management-window duplicate/remove, generic Lock All, normal Exit, and scene/HWND restoration after relaunch. Explorer exposed no usable WorkerW in this session, so the smoke also verified the safe top-level fallback.
- A 30-second idle sample of that nine-widget Debug scene measured 0.0293% average CPU, 118,931,456-byte working set, 193,392,640-byte private memory, 24 threads, 438 handles, zero TCP connections, and zero UDP endpoints. Before process-shared rendering factories, the same nine-widget validation shape used 123 threads, 1,135 handles, 194,412,544-byte working set, and 352,243,712-byte private memory.
- CMake install/package creation succeeded at `C:\WidgetStudioBuild\dist\WidgetStudio`. The clean candidate contains only `WidgetStudio.exe`, `portable.mode`, `README.txt`, and empty `assets`/portable `data` directories.
- The current Release executable SHA-256 is `9320D6DBDBD1CA5D8CC645AB2B74870970E5166A3D80B420722758A42B80C795`. It is x64, GUI-subsystem, unsigned, uses the static MSVC runtime, and imports only Windows system DLL/API sets.
- The current source and link definitions contain no Winsock, WinHTTP, WinINet, telemetry, updater, registry-preference, service, or driver implementation.
- Code Integrity events 3077 and 3033 identify policy `{0283ac0f-fff1-49ae-ada1-8a933130cad6}` and an Enterprise signing-level failure for the unsigned runtime-validation executable. No bypass or policy weakening was attempted.

## Required final gates

- Obtain an organization-approved signing or application-control allow solution for freshly built WidgetStudio executables. Do not weaken or bypass Application Control.
- Re-run current Release CTest plus the Release single-instance/portable-scene GUI smoke and idle-resource checks after that exact output is authorized. Equivalent current Debug runtime coverage passes.
- Validate actual WorkerW attachment, tray commands, edit interactions, Widget Library/Studio controls, Photo import, live media metadata/transport, restart restore, Explorer restart recovery, wallpaper changes, and launch-at-login on an interactive Windows 11 desktop.
- Validate mixed-DPI multi-monitor placement, taskbar/work-area changes, monitor disconnect/reconnect, and wallpaper alignment.
- Confirm the measured Debug resource improvement in Release, then promote the inspected release candidate to the final portable package.

WidgetStudio is not complete until every final gate has current evidence.
