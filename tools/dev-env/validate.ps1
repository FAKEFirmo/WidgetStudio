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

$sourcePath = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$resolvedBuildRoot = [IO.Path]::GetFullPath($BuildRoot).TrimEnd('\')
Assert-OutOfTreeBuildPath $sourcePath $resolvedBuildRoot
foreach ($configuration in @('Debug', 'Release')) {
    $configurationPath = [IO.Path]::GetFullPath((Join-Path $resolvedBuildRoot $configuration)).TrimEnd('\')
    if (-not $configurationPath.StartsWith(
            $resolvedBuildRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Build configuration path '$configurationPath' must remain inside '$resolvedBuildRoot'."
    }
    if (Test-Path -LiteralPath $configurationPath) {
        Remove-Item -LiteralPath $configurationPath -Recurse -Force
    }
}

$build = Join-Path $PSScriptRoot 'build.ps1'
& $build -Configuration Debug -BuildRoot $BuildRoot -CMakePath $cmake -NinjaPath $ninja -VsDevCmdPath $VsDevCmdPath
& $build -Configuration Release -BuildRoot $BuildRoot -CMakePath $cmake -NinjaPath $ninja -VsDevCmdPath $VsDevCmdPath

$distribution = Join-Path $BuildRoot 'dist'
& (Join-Path $PSScriptRoot '..\package.ps1') -BuildPath (Join-Path $BuildRoot 'Release') -OutputPath $distribution -CMakePath $cmake

# Debug and Release tests are independent evidence. Run both even if one is
# rejected by local application-control policy, but never continue to runtime
# validation when either configuration did not pass.
$testFailures = [Collections.Generic.List[string]]::new()
foreach ($configuration in @('Debug', 'Release')) {
    try {
        & (Join-Path $PSScriptRoot 'test.ps1') -Configuration $configuration -BuildRoot $BuildRoot -CTestPath $ctest
    } catch {
        $testFailures.Add("$configuration`: $($_.Exception.Message)")
        Write-Warning "$configuration CTest did not pass: $($_.Exception.Message)"
    }
}
if ($testFailures.Count -gt 0) {
    throw "CTest validation failed:`n$($testFailures -join "`n")"
}

if (-not $SkipRuntime) {
    # Never launch the final distribution in place: portable mode would write
    # first-run state into the artifact that is meant to be delivered clean.
    $runtimeRoot = Join-Path $BuildRoot 'runtime-validation'
    $resolvedRuntimeRoot = [IO.Path]::GetFullPath($runtimeRoot).TrimEnd('\')
    if (-not $resolvedRuntimeRoot.StartsWith(
            $resolvedBuildRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Runtime validation path '$resolvedRuntimeRoot' must remain inside '$resolvedBuildRoot'."
    }
    if (Test-Path -LiteralPath $resolvedRuntimeRoot) {
        Remove-Item -LiteralPath $resolvedRuntimeRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $resolvedRuntimeRoot | Out-Null
    Copy-Item -LiteralPath (Join-Path $distribution 'WidgetStudio') -Destination $resolvedRuntimeRoot -Recurse
    $executable = Join-Path $resolvedRuntimeRoot 'WidgetStudio\WidgetStudio.exe'
    $reports = Join-Path $BuildRoot 'reports'
    New-Item -ItemType Directory -Force -Path $reports | Out-Null
    & (Join-Path $PSScriptRoot 'smoke.ps1') -ExecutablePath $executable -ReportPath (Join-Path $reports 'smoke-report.json')
    & (Join-Path $PSScriptRoot 'performance.ps1') -ExecutablePath $executable -ReportPath (Join-Path $reports 'performance-report.json')
}

Write-Host "Native validation completed. Outputs are under $BuildRoot"
