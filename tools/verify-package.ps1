[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ArchivePath,

    [switch]$RequireValidSignature
)

$ErrorActionPreference = 'Stop'
$resolvedArchive = (Resolve-Path -LiteralPath $ArchivePath).Path
$scratchRoot = Join-Path ([IO.Path]::GetTempPath()) ('WidgetStudioVerify-' + [Guid]::NewGuid().ToString('N'))

try {
    New-Item -ItemType Directory -Path $scratchRoot | Out-Null
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory($resolvedArchive, $scratchRoot)

    $packageRoot = Join-Path $scratchRoot 'WidgetStudio'
    foreach ($required in @(
            'WidgetStudio.exe',
            'portable.mode',
            'VERSION.txt',
            'README.txt',
            'LICENSE',
            'assets',
            'data\config',
            'data\images',
            'data\cache')) {
        if (-not (Test-Path -LiteralPath (Join-Path $packageRoot $required))) {
            throw "The portable archive is missing '$required'."
        }
    }

    $unexpectedRoots = @(Get-ChildItem -LiteralPath $scratchRoot | Where-Object Name -ne 'WidgetStudio')
    if ($unexpectedRoots.Count -gt 0) {
        throw "The archive has unexpected root entries: $($unexpectedRoots.Name -join ', ')"
    }

    $forbiddenExtensions = @('.obj', '.pdb', '.ilk', '.lib', '.exp')
    $forbiddenFiles = @(Get-ChildItem -LiteralPath $packageRoot -Recurse -File | Where-Object {
        $forbiddenExtensions -contains $_.Extension.ToLowerInvariant()
    })
    if ($forbiddenFiles.Count -gt 0) {
        throw "The portable archive contains development artifacts: $($forbiddenFiles.FullName -join ', ')"
    }

    $packagedRuntimeData = @(Get-ChildItem -LiteralPath (Join-Path $packageRoot 'data') -Recurse -File)
    if ($packagedRuntimeData.Count -gt 0) {
        throw "The portable archive contains runtime state: $($packagedRuntimeData.FullName -join ', ')"
    }

    $executable = Join-Path $packageRoot 'WidgetStudio.exe'
    if ($RequireValidSignature) {
        $signature = Get-AuthenticodeSignature -LiteralPath $executable
        if ($signature.Status -ne [Management.Automation.SignatureStatus]::Valid) {
            throw "WidgetStudio.exe does not have a valid Authenticode signature: $($signature.StatusMessage)"
        }
        Write-Host "Signer: $($signature.SignerCertificate.Subject)"
    }

    Write-Host "Portable archive verified: $resolvedArchive"
    Write-Host "SHA-256: $((Get-FileHash -LiteralPath $resolvedArchive -Algorithm SHA256).Hash)"
}
finally {
    if (Test-Path -LiteralPath $scratchRoot) {
        Remove-Item -LiteralPath $scratchRoot -Recurse -Force
    }
}
