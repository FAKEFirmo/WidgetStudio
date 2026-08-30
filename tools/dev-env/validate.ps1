[CmdletBinding()]
param(
    [string]$BuildRoot = 'C:\WidgetStudioBuild',
    [switch]$SkipRuntime,
    [string]$CMakePath,
    [string]$NinjaPath,
    [string]$VsDevCmdPath
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
$cmake = Resolve-Executable $CMakePath 'cmake' 'cmake.exe'
$ninja = Resolve-Executable $NinjaPath 'ninja' 'ninja.exe'
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
if (-not (Test-Path -LiteralPath $ctest -PathType Leaf)) {
    $ctest = Resolve-Executable '' 'ctest' 'ctest.exe'
}

$build = Join-Path $PSScriptRoot 'build.ps1'
& $build -Configuration Debug -BuildRoot $BuildRoot -CMakePath $cmake -NinjaPath $ninja -VsDevCmdPath $VsDevCmdPath
& (Join-Path $PSScriptRoot 'test.ps1') -Configuration Debug -BuildRoot $BuildRoot -CTestPath $ctest
& $build -Configuration Release -BuildRoot $BuildRoot -CMakePath $cmake -NinjaPath $ninja -VsDevCmdPath $VsDevCmdPath

$distribution = Join-Path $BuildRoot 'dist'
& (Join-Path $PSScriptRoot '..\package.ps1') -BuildPath (Join-Path $BuildRoot 'Release') -OutputPath $distribution -CMakePath $cmake

if (-not $SkipRuntime) {
    $executable = Join-Path $distribution 'WidgetStudio\WidgetStudio.exe'
    $reports = Join-Path $BuildRoot 'reports'
    New-Item -ItemType Directory -Force -Path $reports | Out-Null
    & (Join-Path $PSScriptRoot 'smoke.ps1') -ExecutablePath $executable -ReportPath (Join-Path $reports 'smoke-report.json')
    & (Join-Path $PSScriptRoot 'performance.ps1') -ExecutablePath $executable -ReportPath (Join-Path $reports 'performance-report.json')
}

Write-Host "Native validation completed. Outputs are under $BuildRoot"
