[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackagePath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$resolvedPackage = (Resolve-Path -LiteralPath $PackagePath).Path.TrimEnd('\')
$resolvedOutput = [IO.Path]::GetFullPath($OutputPath)
$outputDirectory = Split-Path -Parent $resolvedOutput

if (-not (Test-Path -LiteralPath (Join-Path $resolvedPackage 'WidgetStudio.exe') -PathType Leaf)) {
    throw "'$resolvedPackage' is not a WidgetStudio portable package."
}
if ($resolvedOutput.StartsWith($resolvedPackage + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The archive output must remain outside the package directory.'
}

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
if (Test-Path -LiteralPath $resolvedOutput) {
    Remove-Item -LiteralPath $resolvedOutput -Force
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory(
    $resolvedPackage,
    $resolvedOutput,
    [IO.Compression.CompressionLevel]::Optimal,
    $true)

Write-Host "Portable archive created at $resolvedOutput"
Write-Host "SHA-256: $((Get-FileHash -LiteralPath $resolvedOutput -Algorithm SHA256).Hash)"
