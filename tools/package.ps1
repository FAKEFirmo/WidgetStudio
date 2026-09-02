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
$outputRoot = [IO.Path]::GetFullPath($OutputPath).TrimEnd('\')
$resolvedReleaseRoot = [IO.Path]::GetFullPath($releaseRoot).TrimEnd('\')
if (-not $resolvedReleaseRoot.StartsWith($outputRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw "Portable release path '$resolvedReleaseRoot' must remain inside '$outputRoot'."
}
if (Test-Path -LiteralPath $resolvedReleaseRoot) {
    Remove-Item -LiteralPath $resolvedReleaseRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $releaseRoot | Out-Null

& $CMakePath --install $BuildPath --config Release --prefix $releaseRoot
if ($LASTEXITCODE -ne 0) {
    throw "CMake install failed with exit code $LASTEXITCODE."
}

$executable = Join-Path $releaseRoot 'WidgetStudio.exe'
if (-not (Test-Path -LiteralPath $executable)) {
    throw "The portable release executable was not produced at $executable."
}

foreach ($required in @(
        'portable.mode',
        'VERSION.txt',
        'README.txt',
        'LICENSE',
        'assets',
        'data\config',
        'data\images',
        'data\cache')) {
    if (-not (Test-Path -LiteralPath (Join-Path $releaseRoot $required))) {
        throw "The portable release is missing '$required'."
    }
}
$forbiddenExtensions = @('.obj', '.pdb', '.ilk', '.lib', '.exp')
$forbiddenFiles = @(Get-ChildItem -LiteralPath $releaseRoot -Recurse -File | Where-Object {
    $forbiddenExtensions -contains $_.Extension.ToLowerInvariant()
})
if ($forbiddenFiles.Count -gt 0) {
    throw "The portable release contains development artifacts: $($forbiddenFiles.FullName -join ', ')"
}
$packagedRuntimeData = @(Get-ChildItem -LiteralPath (Join-Path $releaseRoot 'data') -Recurse -File)
if ($packagedRuntimeData.Count -gt 0) {
    throw "The portable release contains runtime state: $($packagedRuntimeData.FullName -join ', ')"
}

Write-Host "Portable release created at $releaseRoot"
