# Contributing to WidgetStudio

WidgetStudio welcomes focused bug fixes, tests, documentation improvements, and changes that advance the milestones in `docs/MILESTONES.md`.

## Development requirements

- Windows 11 x64
- Visual Studio Build Tools with the MSVC C++ workload and a Windows 11 SDK
- CLion with its bundled CMake and Ninja, or equivalent CMake and Ninja executables

Do not add a VM, container, subsystem, package manager, browser runtime, external widget host, or second UI framework to the development or runtime architecture. Keep all generated output outside the source checkout; the scripts default to `C:\WidgetStudioBuild`.

## Before opening a pull request

From PowerShell, run:

```powershell
.\tools\dev-env\build.ps1 -Configuration Debug
.\tools\dev-env\test.ps1 -Configuration Debug
.\tools\dev-env\build.ps1 -Configuration Release
.\tools\dev-env\test.ps1 -Configuration Release
```

For UI or Windows integration changes, also run the application on Windows 11 when local policy permits it. Describe anything that could not be exercised.

Keep changes within the boundaries in `AGENTS.md` and `docs/ARCHITECTURE.md`. New widget types must be registered through the existing descriptor/factory system; widget code must not take ownership of general positioning, selection, or drag behavior.

## Pull requests

- Keep each pull request centered on one coherent change.
- Add or update tests for domain logic.
- Keep the build warning-clean where practical.
- Do not commit generated output, runtime data, credentials, certificates, tokens, or private keys.
- Explain behavior changes, validation performed, and remaining risks.

By contributing, you agree that your contribution is licensed under the repository's MIT License.
