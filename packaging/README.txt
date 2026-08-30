WidgetStudio 1.0 portable release

Run WidgetStudio.exe. No installation or administrator access is required.

This release includes portable.mode, so configuration and imported photos are
stored under portable-data beside the executable. Delete the whole release
folder to remove the application and its portable user data.

Launch at login is disabled by default. If enabled from the tray menu, the app
creates one removable WidgetStudio.lnk in the current user's Startup folder.
Turn the option off before deleting this folder for a residue-free removal.

WidgetStudio performs no telemetry, update checks, or network requests.
It installs no service, driver, updater, registry configuration, or external
widget host.

The normal windowed desktop backend is used by default. The optional,
experimental Explorer WorkerW backend can be requested for one launch by
setting WIDGETSTUDIO_DESKTOP_BACKEND=workerw in that process environment.
