WidgetStudio 1.0 portable release

Run WidgetStudio.exe. No installation or administrator access is required.

This release includes portable.mode, so configuration and imported photos are
stored under portable-data beside the executable. Delete the whole release
folder to remove the application and its portable user data.

WidgetStudio performs no telemetry, update checks, or network requests.

The normal windowed desktop backend is used by default. The optional,
experimental Explorer WorkerW backend can be requested for one launch by
setting WIDGETSTUDIO_DESKTOP_BACKEND=workerw in that process environment.
