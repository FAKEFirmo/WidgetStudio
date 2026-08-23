[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$devRoot = 'C:\WidgetStudioDev'
$installerPath = Join-Path $devRoot 'vs_BuildTools.exe'
$installPath = 'C:\BuildTools'
$sourcePath = 'C:\Workspace\WidgetStudio'
$outputPath = 'C:\Workspace\out'
$logPath = Join-Path $outputPath 'bootstrap.log'

New-Item -ItemType Directory -Force -Path $devRoot, $outputPath | Out-Null
Start-Transcript -LiteralPath $logPath -Force

try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -UseBasicParsing -Uri 'https://aka.ms/vs/17/release/vs_BuildTools.exe' -OutFile $installerPath

    $installArguments = @(
        '--quiet',
        '--wait',
        '--norestart',
        '--nocache',
        '--installPath', $installPath,
        '--add', 'Microsoft.VisualStudio.Workload.VCTools',
        '--includeRecommended'
    )
    $installer = Start-Process -FilePath $installerPath -ArgumentList $installArguments -Wait -PassThru
    if ($installer.ExitCode -notin 0, 3010) {
        throw "Visual Studio Build Tools setup failed with exit code $($installer.ExitCode)."
    }

    & (Join-Path $sourcePath 'tools\dev-env\build.ps1') `
        -SourcePath $sourcePath `
        -BuildPath (Join-Path $devRoot 'build') `
        -OutputPath $outputPath `
        -Configuration Debug

    & (Join-Path $sourcePath 'tools\dev-env\test.ps1') `
        -BuildPath (Join-Path $devRoot 'build') `
        -Configuration Debug

    & (Join-Path $sourcePath 'tools\dev-env\build.ps1') `
        -SourcePath $sourcePath `
        -BuildPath (Join-Path $devRoot 'build') `
        -OutputPath $outputPath `
        -Configuration Release

    & (Join-Path $sourcePath 'tools\package.ps1') `
        -BuildPath (Join-Path $devRoot 'build') `
        -OutputPath (Join-Path $outputPath 'dist') `
        -CMakePath 'C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'

    $releaseExecutable = Join-Path $outputPath 'dist\WidgetStudio\WidgetStudio.exe'
    & (Join-Path $sourcePath 'tools\dev-env\smoke.ps1') `
        -ExecutablePath $releaseExecutable `
        -ReportPath (Join-Path $outputPath 'smoke-report.json')

    & (Join-Path $sourcePath 'tools\dev-env\performance.ps1') `
        -ExecutablePath $releaseExecutable `
        -ReportPath (Join-Path $outputPath 'performance-report.json')

    Write-Host 'WidgetStudio sandbox build and tests completed.'
    Write-Host "Artifacts were copied to $outputPath on the host mapping."
} finally {
    Stop-Transcript
}
