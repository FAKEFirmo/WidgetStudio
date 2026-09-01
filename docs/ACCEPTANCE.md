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
- Current clean Debug and Release CTest each passed 1/1.
- The current-source portable Debug smoke passed with all production widgets present. It verified one guarded process, registry enumeration, all four production widget types, multiple instances, matching per-widget HWND counts, passive `HTTRANSPARENT` margins, Widget Studio preview-above-settings layout, safe Grid-to-Free conversion, universal appearance toggles, a Clock-specific setting, state-preserving duplication, removal, generic Lock All, normal Exit, and scene/HWND restoration after relaunch. Explorer exposed no usable WorkerW in this session, so the smoke also verified the safe top-level fallback.
- A 30-second idle sample of that nine-widget Debug scene measured 0.0293% average CPU, 118,931,456-byte working set, 193,392,640-byte private memory, 24 threads, 438 handles, zero TCP connections, and zero UDP endpoints. Before process-shared rendering factories, the same nine-widget validation shape used 123 threads, 1,135 handles, 194,412,544-byte working set, and 352,243,712-byte private memory.
- CMake install/package creation succeeded at `C:\WidgetStudioBuild\publication-validation\dist\WidgetStudio`. The clean candidate contains `WidgetStudio.exe`, `portable.mode`, `README.txt`, the MIT `LICENSE`, and empty `assets`/portable `data` directories.
- The external-tree publication validation rebuilt Debug and Release and passed both CTests on 2026-09-01. `tools/archive-package.ps1` created a root-preserving portable ZIP and `tools/verify-package.ps1` accepted it; that local unsigned archive's SHA-256 was `20E9EB18870DC7EFAB581212556A198FFE3A58F8F3562ADD5BC8EB63BE17A700`.
- `.github/workflows/windows-ci.yml` now expresses the same Debug/Release/CTest/package path on GitHub-hosted Windows and contains a gated SignPath submission path. The remote workflow and SignPath integration have not yet run because the public repository and Foundation project do not exist.
- The publication-validation Release executable SHA-256 is `16045E31DFE21118EE2B47EF93F8747CACC2782A94BA35B2E574417F328790C3`. It is x64, GUI-subsystem, unsigned, uses the static MSVC runtime, and imports only Windows system DLL/API sets.
- The current source and link definitions contain no Winsock, WinHTTP, WinINet, telemetry, updater, registry-preference, service, or driver implementation.
- Code Integrity events 3077 and 3033 identify policy `{0283ac0f-fff1-49ae-ada1-8a933130cad6}` and an Enterprise signing-level failure for the unsigned runtime-validation executable. No bypass or policy weakening was attempted.

## Required final gates

- Publish the repository, complete the documented SignPath Foundation setup, and obtain a signed `WidgetStudio-portable.zip` from the tagged GitHub workflow. Do not weaken or bypass Application Control.
- Download that exact signed ZIP without rebuilding or repackaging it, verify its Authenticode signature and recorded SHA-256, then run the Release single-instance/portable-scene GUI smoke and idle-resource checks. Both current clean CTests and equivalent current-source Debug runtime coverage pass.
- Validate actual WorkerW attachment, tray commands, edit interactions, Widget Library/Studio controls, Photo import, live media metadata/transport, restart restore, Explorer restart recovery, wallpaper changes, and launch-at-login on an interactive Windows 11 desktop.
- Validate mixed-DPI multi-monitor placement, taskbar/work-area changes, monitor disconnect/reconnect, and wallpaper alignment.
- Confirm the measured Debug resource improvement in Release, then promote the inspected release candidate to the final portable package.

WidgetStudio is not complete until every final gate has current evidence.
