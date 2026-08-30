$ErrorActionPreference = 'Stop'

function Resolve-Executable([string]$ExplicitPath, [string]$CommandName, [string]$LeafName) {
    if ($ExplicitPath) {
        if (-not (Test-Path -LiteralPath $ExplicitPath -PathType Leaf)) {
            throw "$LeafName was not found at '$ExplicitPath'."
        }
        return (Resolve-Path -LiteralPath $ExplicitPath).Path
    }

    $command = Get-Command $CommandName -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $standardClion = Join-Path $env:LOCALAPPDATA 'Programs\CLion'
    $relativeToolPath = switch ($LeafName) {
        'ninja.exe' { 'bin\ninja\win\x64\ninja.exe' }
        default { "bin\cmake\win\x64\bin\$LeafName" }
    }
    $standardTool = Join-Path $standardClion $relativeToolPath
    if (Test-Path -LiteralPath $standardTool -PathType Leaf) { return $standardTool }

    $clionRoots = @(
        (Join-Path $env:ProgramFiles 'JetBrains'),
        (Join-Path $env:LOCALAPPDATA 'Programs'),
        (Join-Path $env:LOCALAPPDATA 'JetBrains\Toolbox\apps\CLion')
    ) | Where-Object { Test-Path -LiteralPath $_ }

    foreach ($root in $clionRoots) {
        $match = Get-ChildItem -LiteralPath $root -Recurse -File -Filter $LeafName -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '[\\/]CLion' } |
            Select-Object -First 1
        if ($match) { return $match.FullName }
    }

    throw "$LeafName was not found. Install/configure CLion with its bundled CMake and Ninja, or pass an explicit path."
}

function Import-MsvcEnvironment([string]$VsDevCmdPath) {
    if (-not $VsDevCmdPath) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) {
            throw 'MSVC Build Tools were not found. Install the Visual Studio Build Tools C++ workload and a Windows 11 SDK.'
        }
        $installationPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($LASTEXITCODE -ne 0 -or -not $installationPath) {
            throw 'MSVC Build Tools with the x64 C++ compiler were not found.'
        }
        $VsDevCmdPath = Join-Path $installationPath 'Common7\Tools\VsDevCmd.bat'
    }
    if (-not (Test-Path -LiteralPath $VsDevCmdPath -PathType Leaf)) {
        throw "VsDevCmd.bat was not found at '$VsDevCmdPath'."
    }

    $command = "`"$VsDevCmdPath`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
    $environment = & $env:ComSpec /d /s /c $command
    if ($LASTEXITCODE -ne 0) { throw "VsDevCmd.bat failed with exit code $LASTEXITCODE." }
    foreach ($line in $environment) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            [Environment]::SetEnvironmentVariable(
                $line.Substring(0, $separator), $line.Substring($separator + 1), 'Process')
        }
    }
    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw 'VsDevCmd.bat completed, but cl.exe is not available in the process environment.'
    }
}

function Assert-OutOfTreeBuildPath([string]$SourcePath, [string]$BuildPath) {
    $source = [IO.Path]::GetFullPath($SourcePath).TrimEnd('\')
    $build = [IO.Path]::GetFullPath($BuildPath).TrimEnd('\')
    if ($build.Equals($source, [StringComparison]::OrdinalIgnoreCase) -or
        $build.StartsWith($source + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Build output must remain outside the source repository. Received '$build'."
    }
}
