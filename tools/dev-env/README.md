# Native Windows development

WidgetStudio uses the smallest normal Windows toolchain: CLion, MSVC Build Tools with the x64 C++ workload, and a Windows 11 SDK. CMake and Ninja normally come from CLion. On machines where Application Control blocks JetBrains executables, enable Visual Studio's optional **C++ CMake tools for Windows** component and point CLion at those Microsoft-signed CMake/Ninja copies. No package manager, VM, container, subsystem, service, or globally installed library is part of this workflow.

CLion profiles must keep their generation directory outside the repository. Use `C:\WidgetStudioBuild\clion-debug` and `C:\WidgetStudioBuild\clion-release`, select the Visual Studio/MSVC toolchain (not CLion's bundled MinGW toolchain), and use the Ninja generator.

For command-line validation from an ordinary PowerShell prompt:

```powershell
.\tools\dev-env\validate.ps1
```

The scripts locate Visual Studio through `vswhere`, import its x64 environment into the script process only, and resolve CMake/Ninja from PATH, Visual Studio's CMake component, or CLion in that order. They never modify the machine PATH. Explicit `-CMakePath`, `-NinjaPath`, and `-VsDevCmdPath` arguments are available for nonstandard installations.

Outputs are isolated under `C:\WidgetStudioBuild` by default:

- `Debug` and `Release`: independent Ninja build trees
- `dist\WidgetStudio`: portable release
- `reports`: GUI smoke and warm-idle performance reports

Use `-SkipRuntime` only when GUI execution is unavailable. Deleting `C:\WidgetStudioBuild` removes all generated output; no build file is written into the source repository.
