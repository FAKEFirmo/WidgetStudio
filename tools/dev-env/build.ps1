[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$BuildRoot = 'C:\WidgetStudioBuild',
    [string]$CMakePath,
    [string]$NinjaPath,
    [string]$VsDevCmdPath
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$sourcePath = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildPath = Join-Path $BuildRoot $Configuration
Assert-OutOfTreeBuildPath $sourcePath $buildPath
Import-MsvcEnvironment $VsDevCmdPath
$cmake = Resolve-Executable $CMakePath 'cmake' 'cmake.exe'
$ninja = Resolve-Executable $NinjaPath 'ninja' 'ninja.exe'

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null
& $cmake -S $sourcePath -B $buildPath -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_MAKE_PROGRAM=$ninja"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE." }

& $cmake --build $buildPath
if ($LASTEXITCODE -ne 0) { throw "CMake build failed with exit code $LASTEXITCODE." }
Write-Host "$Configuration build completed at $buildPath"
