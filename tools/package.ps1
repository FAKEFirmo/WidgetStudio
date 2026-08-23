[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [string]$CMakePath = 'cmake'
)

$ErrorActionPreference = 'Stop'
$releaseRoot = Join-Path $OutputPath 'WidgetStudio'
New-Item -ItemType Directory -Force -Path $releaseRoot | Out-Null

& $CMakePath --install $BuildPath --config Release --prefix $releaseRoot
if ($LASTEXITCODE -ne 0) {
    throw "CMake install failed with exit code $LASTEXITCODE."
}

$executable = Join-Path $releaseRoot 'WidgetStudio.exe'
if (-not (Test-Path -LiteralPath $executable)) {
    throw "The portable release executable was not produced at $executable."
}

Write-Host "Portable release created at $releaseRoot"
