# Release and operation guide

WidgetStudio is a native Windows 11 x64 application. One `WidgetStudio.exe` process owns the tray controller, registry, scene, persistence, management windows, media integration, and one lightweight HWND for each widget instance. There is no external widget host or runtime third-party dependency.

## Development prerequisites

- CLion
- Visual Studio Build Tools or Visual Studio Community with MSVC x64 C++ tools
- A Windows 11 SDK
- CMake and Ninja supplied by CLion, or Visual Studio's **C++ CMake tools for Windows** component when Application Control rejects the CLion copies

No package manager, VM, container, WSL distribution, service, driver, or globally installed library is required.

## CLion setup

Select the Visual Studio/MSVC x64 toolchain and the Ninja generator. Keep generation directories outside the repository, for example:

- Debug: `C:\WidgetStudioBuild\clion-debug`
- Release: `C:\WidgetStudioBuild\clion-release`

Open the repository's `CMakeLists.txt` directly. Do not use a second project definition.

## Command-line build and validation

From an ordinary PowerShell prompt in the repository:

```powershell
.\tools\dev-env\build.ps1 -Configuration Debug
.\tools\dev-env\test.ps1 -Configuration Debug
.\tools\dev-env\build.ps1 -Configuration Release
```

The complete clean build, test, package, smoke, and idle-resource path is:

```powershell
.\tools\dev-env\validate.ps1
```

Generated files remain under `C:\WidgetStudioBuild`. The current portable release-candidate path is `C:\WidgetStudioBuild\dist\WidgetStudio`. Runtime checks use a separate `runtime-validation` copy so the distribution stays clean. `validate.ps1` records CTest results for both configurations, then intentionally stops before GUI checks if either test is rejected by security policy; it does not attempt a bypass.

## Debugging

Run `WidgetStudio.exe` from the Debug build tree in CLion. The Debug configuration registers a diagnostic widget; Release does not. Set `WIDGETSTUDIO_DESKTOP_BACKEND=windowed` only for a process-local diagnostic run when WorkerW behavior must be isolated. This environment variable is not installed or persisted by WidgetStudio.

## Portable runtime layout

```text
WidgetStudio\
|-- WidgetStudio.exe
|-- portable.mode
|-- README.txt
|-- assets\
`-- data\
    |-- config\
    |-- images\
    `-- cache\
```

`portable.mode` keeps scene configuration, imported photos, and cache data below `data`. Without that sentinel, the corresponding directories are below `%LOCALAPPDATA%\WidgetStudio`.

## Startup and removal

Launch-at-login is off by default. Enabling it creates only the current user's removable `WidgetStudio.lnk` in the Startup folder. Disabling it deletes that shortcut; no Run registry value, service, or scheduled task is used.

For residue-free removal, disable launch-at-login if it was enabled, exit WidgetStudio, and delete the portable folder. Deleting the folder also deletes its portable runtime data.

## Dependencies and Windows APIs

WidgetStudio links only Windows system libraries. Its principal APIs are Win32/User32/Shell, Direct2D, DirectWrite, WIC, SHCore monitor DPI, COM shell links and file dialogs, and C++/WinRT Global System Media Transport Controls. Release uses the static MSVC runtime. The application contains no analytics, telemetry, update client, HTTP client, external plugin loader, or executable widget extension mechanism.

## Platform behavior and known limitations

- Windows 11 x64 is the release target and required final-validation platform.
- Most APIs also exist on supported Windows 10 builds, but Windows 10 is not the final acceptance target. Segoe UI is used if Segoe UI Variable is unavailable.
- Explorer WorkerW attachment is undocumented. Widget windows retry after Explorer restart and fall back without crashing when the host is unavailable.
- Direct wallpaper decoding depends on Windows exposing a decodable current-wallpaper path. A neutral surface is used if it does not.
- Media metadata, artwork, and enabled transport commands depend on what the active Windows media session publishes.
- The global `Ctrl+Alt+W` shortcut can be unavailable if another application has already registered it; tray commands remain available.
- Application Control policy `{0283ac0f-fff1-49ae-ada1-8a933130cad6}` currently requires an Enterprise signing level. Both clean CTests are authorized, while the latest unsigned Release GUI application is blocked from both its build-tree and packaged locations. Use an organization-approved signing or narrowly scoped allow solution; WidgetStudio does not weaken or bypass that policy.

Current implementation evidence and every still-required runtime gate are tracked in [ACCEPTANCE.md](ACCEPTANCE.md). Do not treat the intended release path as final until that checklist has current Debug, Release, CTest, interactive Windows 11, and performance evidence.
