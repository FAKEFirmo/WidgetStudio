[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$BuildRoot = 'C:\WidgetStudioBuild',
    [string]$CTestPath
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
$ctest = Resolve-Executable $CTestPath 'ctest' 'ctest.exe'
$buildPath = Join-Path $BuildRoot $Configuration

& $ctest --test-dir $buildPath --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "CTest failed with exit code $LASTEXITCODE." }
