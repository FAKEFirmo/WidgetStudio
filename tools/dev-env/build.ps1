[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourcePath,

    [Parameter(Mandatory = $true)]
    [string]$BuildPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$cmake = 'C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) {
    throw "The sandbox-local CMake executable was not found at $cmake."
}

New-Item -ItemType Directory -Force -Path $BuildPath, $OutputPath | Out-Null

& $cmake -S $SourcePath -B $BuildPath -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE." }

& $cmake --build $BuildPath --config $Configuration
if ($LASTEXITCODE -ne 0) { throw "CMake build failed with exit code $LASTEXITCODE." }

$binaryDirectory = Join-Path $BuildPath $Configuration
$artifactDirectory = Join-Path $OutputPath $Configuration
New-Item -ItemType Directory -Force -Path $artifactDirectory | Out-Null
Copy-Item -LiteralPath (Join-Path $binaryDirectory 'WidgetStudio.exe') -Destination $artifactDirectory -Force

$pdb = Join-Path $binaryDirectory 'WidgetStudio.pdb'
if (Test-Path -LiteralPath $pdb) {
    Copy-Item -LiteralPath $pdb -Destination $artifactDirectory -Force
}
