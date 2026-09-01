# Code signing policy

Free code signing provided by [SignPath.io](https://signpath.io/), certificate by [SignPath Foundation](https://signpath.org/).

## Signed artifacts

Only `WidgetStudio.exe` built from this repository may receive the WidgetStudio Authenticode signature. The surrounding portable ZIP contains that executable, `portable.mode`, the runtime README, and application-owned data directories. It contains no installer, updater, service, driver, external widget host, or privately supplied binary.

The repository's Windows workflow builds and tests Debug and Release configurations with MSVC, CMake, Ninja, and CTest on GitHub-hosted runners. It uploads the exact Release ZIP to GitHub before sending its immutable artifact ID to SignPath. The signing job is available only for an explicit manual run selected at a `v*` tag, remains disabled until the SignPath project is configured, and waits for the required human approval. It then verifies the returned executable's Authenticode signature before publishing the signed ZIP as a workflow artifact.

## Team roles

- Authors: contributors identified by the public Git history. Direct write access is limited to the maintainers named below.
- Committers and reviewers: `@FAKEFirmo`
- Approvers: `SIGNPATH_APPROVER` — I'll replace later

All members with repository or SignPath access must use multi-factor authentication. Changes from people without direct write access require maintainer review. Signing requests require manual approval by an Approver.

## Privacy policy

This program will not transfer any information to other networked systems unless specifically requested by the user or the person installing or operating it.

WidgetStudio has no telemetry, updater, background web service, or default network feature. Its initial widgets use local system time, local calendar calculations, local images, the Windows wallpaper, and Windows media-session APIs. Runtime state remains on the local computer and, in portable mode, under the application's own `data` directory.

## Release verification

For a signed release, maintainers publish the exact `WidgetStudio-portable.zip` returned by the signing workflow; they do not rebuild or repackage it. The workflow records its source commit and signed ZIP SHA-256 digest. Final acceptance is performed from that same downloaded ZIP on Windows 11 and includes Authenticode verification plus the GUI/runtime checks in `docs/ACCEPTANCE.md`.

The placeholders in this document are intentional while the repository is still local. Signing must remain disabled until both role entries identify real people or public GitHub teams.
