[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$ReportPath,

    [int]$WarmupSeconds = 10,
    [int]$SampleSeconds = 30,
    [double]$MaximumAverageCpuPercent = 0.50
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $ExecutablePath)) {
    throw "WidgetStudio executable was not found at $ExecutablePath."
}

$process = Start-Process -FilePath $ExecutablePath -PassThru
try {
    Start-Sleep -Seconds $WarmupSeconds
    $process.Refresh()
    if ($process.HasExited) { throw 'WidgetStudio exited during the performance warmup.' }

    $startCpu = $process.TotalProcessorTime
    $start = [DateTime]::UtcNow
    Start-Sleep -Seconds $SampleSeconds
    $process.Refresh()
    if ($process.HasExited) { throw 'WidgetStudio exited during the performance sample.' }

    $elapsed = ([DateTime]::UtcNow - $start).TotalSeconds
    $cpuSeconds = ($process.TotalProcessorTime - $startCpu).TotalSeconds
    $logicalProcessors = [Math]::Max(1, [Environment]::ProcessorCount)
    $averageCpuPercent = 100.0 * $cpuSeconds / ($elapsed * $logicalProcessors)
    $tcpConnectionCount = $null
    if (Get-Command Get-NetTCPConnection -ErrorAction SilentlyContinue) {
        $tcpConnectionCount = @(Get-NetTCPConnection -OwningProcess $process.Id -ErrorAction SilentlyContinue).Count
    }

    $report = [ordered]@{
        passed = $averageCpuPercent -le $MaximumAverageCpuPercent
        sampleSeconds = [Math]::Round($elapsed, 3)
        averageCpuPercent = [Math]::Round($averageCpuPercent, 4)
        maximumAverageCpuPercent = $MaximumAverageCpuPercent
        workingSetBytes = $process.WorkingSet64
        privateMemoryBytes = $process.PrivateMemorySize64
        threadCount = $process.Threads.Count
        tcpConnectionCount = $tcpConnectionCount
        observedAtUtc = [DateTime]::UtcNow.ToString('o')
    }
    $report | ConvertTo-Json | Set-Content -LiteralPath $ReportPath -Encoding UTF8
    if (-not $report.passed) {
        throw "Average idle CPU $averageCpuPercent% exceeded $MaximumAverageCpuPercent%."
    }
} finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
}
