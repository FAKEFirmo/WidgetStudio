[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $ExecutablePath)) {
    throw "WidgetStudio executable was not found at $ExecutablePath."
}

$process = Start-Process -FilePath $ExecutablePath -PassThru
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
    } while (-not $process.HasExited -and $process.MainWindowHandle -eq 0 -and [DateTime]::UtcNow -lt $deadline)

    if ($process.HasExited) {
        throw "WidgetStudio exited during startup with code $($process.ExitCode)."
    }
    if ($process.MainWindowHandle -eq 0) {
        throw 'WidgetStudio did not create its main window within 15 seconds.'
    }
    $scenePath = Join-Path (Split-Path -Parent $ExecutablePath) 'portable-data\scene.json'
    $sceneDeadline = [DateTime]::UtcNow.AddSeconds(5)
    while (-not (Test-Path -LiteralPath $scenePath) -and [DateTime]::UtcNow -lt $sceneDeadline) {
        Start-Sleep -Milliseconds 250
    }
    if (-not (Test-Path -LiteralPath $scenePath)) {
        throw "WidgetStudio did not create its portable scene at $scenePath."
    }

    $report = [ordered]@{
        passed = $true
        processId = $process.Id
        mainWindowHandle = ('0x{0:X}' -f $process.MainWindowHandle.ToInt64())
        portableScenePath = $scenePath
        observedAtUtc = [DateTime]::UtcNow.ToString('o')
    }
    $report | ConvertTo-Json | Set-Content -LiteralPath $ReportPath -Encoding UTF8
} finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
}
