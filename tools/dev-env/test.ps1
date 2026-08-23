[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildPath,

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$ctest = 'C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
if (-not (Test-Path -LiteralPath $ctest)) {
    throw "The sandbox-local CTest executable was not found at $ctest."
}

& $ctest --test-dir $BuildPath -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "CTest failed with exit code $LASTEXITCODE." }
