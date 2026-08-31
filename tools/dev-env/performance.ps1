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

function Get-NetstatEndpointCount([string]$Protocol, [int]$ProcessId) {
    $netstat = Join-Path $env:SystemRoot 'System32\netstat.exe'
    if (-not (Test-Path -LiteralPath $netstat -PathType Leaf)) { return $null }
    $lines = & $netstat -ano -p $Protocol 2>$null
    if ($LASTEXITCODE -ne 0) { return $null }
    $count = 0
    foreach ($line in $lines) {
        if ($line -match '^\s*(TCP|UDP)\s+.*\s+(\d+)\s*$' -and
            [int]$Matches[2] -eq $ProcessId) {
            ++$count
        }
    }
    return $count
}

$process = Start-Process -FilePath $ExecutablePath -WorkingDirectory (Split-Path -Parent $ExecutablePath) -PassThru
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
    $udpEndpointCount = $null
    if (Get-Command Get-NetTCPConnection -ErrorAction SilentlyContinue) {
        $tcpErrors = @()
        $tcpConnections = @(Get-NetTCPConnection -OwningProcess $process.Id -ErrorAction SilentlyContinue -ErrorVariable tcpErrors)
        if ($tcpErrors.Count -eq 0) { $tcpConnectionCount = $tcpConnections.Count }
    }
    if (Get-Command Get-NetUDPEndpoint -ErrorAction SilentlyContinue) {
        $udpErrors = @()
        $udpEndpoints = @(Get-NetUDPEndpoint -OwningProcess $process.Id -ErrorAction SilentlyContinue -ErrorVariable udpErrors)
        if ($udpErrors.Count -eq 0) { $udpEndpointCount = $udpEndpoints.Count }
    }
    # NetTCPIP cmdlets can be unavailable to a normal user under constrained
    # policy. netstat provides a read-only, inbox fallback keyed by process ID.
    if ($null -eq $tcpConnectionCount) {
        $tcpConnectionCount = Get-NetstatEndpointCount 'tcp' $process.Id
    }
    if ($null -eq $udpEndpointCount) {
        $udpEndpointCount = Get-NetstatEndpointCount 'udp' $process.Id
    }
    $networkPassed = ($null -eq $tcpConnectionCount -or $tcpConnectionCount -eq 0) -and
        ($null -eq $udpEndpointCount -or $udpEndpointCount -eq 0)

    $report = [ordered]@{
        passed = $averageCpuPercent -le $MaximumAverageCpuPercent -and $networkPassed
        sampleSeconds = [Math]::Round($elapsed, 3)
        averageCpuPercent = [Math]::Round($averageCpuPercent, 4)
        maximumAverageCpuPercent = $MaximumAverageCpuPercent
        workingSetBytes = $process.WorkingSet64
        privateMemoryBytes = $process.PrivateMemorySize64
        threadCount = $process.Threads.Count
        handleCount = $process.HandleCount
        tcpConnectionCount = $tcpConnectionCount
        udpEndpointCount = $udpEndpointCount
        networkCheckAvailable = $null -ne $tcpConnectionCount -or $null -ne $udpEndpointCount
        observedAtUtc = [DateTime]::UtcNow.ToString('o')
    }
    $report | ConvertTo-Json | Set-Content -LiteralPath $ReportPath -Encoding UTF8
    if (-not $report.passed) {
        throw "Runtime cleanliness check failed (average CPU $averageCpuPercent%, TCP $tcpConnectionCount, UDP $udpEndpointCount)."
    }
} finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
}
