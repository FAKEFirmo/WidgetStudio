WidgetStudio 1.0 portable release

Run WidgetStudio.exe. No installation or administrator access is required.

This release includes portable.mode, so configuration, imported photos, and
cache data are stored under data beside the executable. Delete the whole
release folder to remove the application and its portable user data.

Temporary startup diagnostics are written as UTF-8 text to
data\logs\startup.log. If startup stops before the tray icon appears, inspect
that file and the process exit code. A second launch exits with code 2 and
records that the single-instance guard is already held.

Launch at login is disabled by default. If enabled from the tray menu, the app
creates one removable WidgetStudio.lnk in the current user's Startup folder.
Turn the option off before deleting this folder for a residue-free removal.

WidgetStudio performs no telemetry, update checks, or network requests.
It installs no service, driver, updater, registry configuration, or external
widget host.

WidgetStudio is open-source software distributed under the MIT License. See
LICENSE in this folder. Official signed releases use Authenticode signing
provided by SignPath.io with a certificate issued by SignPath Foundation.

Each widget uses its own lightweight native window. WidgetStudio attempts
Explorer WorkerW desktop attachment and falls back safely if that undocumented
host is unavailable. Set WIDGETSTUDIO_DESKTOP_BACKEND=windowed only to force
the fallback for diagnostics.
