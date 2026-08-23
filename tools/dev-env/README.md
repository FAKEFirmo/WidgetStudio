# Disposable Windows build environment

This directory defines a Windows Sandbox build path that leaves the host development environment unchanged. The repository is mounted read-only at `C:\Workspace\WidgetStudio`; only `out\sandbox` is writable from the sandbox.

## Host prerequisite

Windows Sandbox must already be available. These scripts do not enable the Windows feature and do not request administrator rights on the host. On the current machine `WindowsSandbox.exe` was not found, so the workflow has not yet been executed.

If you choose to enable Windows Sandbox later, do that explicitly through Windows Features and restart when Windows requests it. That is a host-level change and is intentionally outside these scripts.

## Run

1. Ensure `out\sandbox` exists; it is included in the repository.
2. Double-click `tools\dev-env\WidgetStudio.wsb`.
3. Wait for the PowerShell window inside the sandbox to finish.
4. Read `out\sandbox\bootstrap.log` on the host.
5. Find Debug artifacts under `out\sandbox\Debug`, Release artifacts under `out\sandbox\Release`, and the portable package under `out\sandbox\dist\WidgetStudio`.

The sandbox downloads the official Visual Studio 2022 Build Tools bootstrapper from Microsoft and installs the C++ workload only inside the disposable VM. It configures with the Visual Studio 2022 x64 generator, builds Debug, runs CTest, builds Release, creates the portable package, and copies application artifacts to the mapped output directory.

## Cleanup

Close Windows Sandbox to erase the compiler, SDK, CMake, downloads, build directory, registry entries, and all other sandbox-local state. Delete the contents of `out\sandbox` to remove copied logs and artifacts from the host.

No host PATH, environment variable, service, startup entry, package manager, or development registry setting is changed by this workflow.

## Repository relocation

`WidgetStudio.wsb` contains the current absolute repository path because `.wsb` files do not reliably expand environment variables in mapped-folder declarations. If the repository moves, update both `<HostFolder>` entries before launching it.
