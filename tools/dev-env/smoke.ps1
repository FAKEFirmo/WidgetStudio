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
    } while (-not $process.HasExited -and [DateTime]::UtcNow -lt $deadline -and
        -not (Test-Path -LiteralPath (Join-Path (Split-Path -Parent $ExecutablePath) 'data\config\scene.json')))

    if ($process.HasExited) {
        throw "WidgetStudio exited during startup with code $($process.ExitCode)."
    }
    $scenePath = Join-Path (Split-Path -Parent $ExecutablePath) 'data\config\scene.json'
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
        controllerArchitecture = 'hidden-controller-with-per-widget-HWNDs'
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
