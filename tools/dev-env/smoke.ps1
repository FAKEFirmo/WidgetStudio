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
if (-not (Test-Path -LiteralPath (Join-Path (Split-Path -Parent $ExecutablePath) 'portable.mode'))) {
    throw 'The runtime smoke target is not a portable WidgetStudio layout.'
}

if (-not ('WidgetStudioSmokeNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class WidgetStudioSmokeNative {
    public delegate bool EnumWindowsProc(IntPtr window, IntPtr context);

    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr context);
    [DllImport("user32.dll")]
    private static extern bool EnumChildWindows(IntPtr parent, EnumWindowsProc callback, IntPtr context);
    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(IntPtr window, StringBuilder name, int capacity);
    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern IntPtr GetDlgItem(IntPtr parent, int id);
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")]
    public static extern IntPtr GetParent(IntPtr window);
    [DllImport("user32.dll")]
    public static extern int GetSystemMetrics(int index);
    public static string ClassNameOf(IntPtr window) {
        if (window == IntPtr.Zero) return "";
        var name = new StringBuilder(256);
        return GetClassName(window, name, name.Capacity) > 0 ? name.ToString() : "";
    }

    private static void AddIfMatch(List<IntPtr> result, IntPtr window, uint processId, string className) {
        uint owner;
        GetWindowThreadProcessId(window, out owner);
        if (owner != processId) return;
        var name = new StringBuilder(256);
        if (GetClassName(window, name, name.Capacity) > 0 && name.ToString() == className) result.Add(window);
    }

    public static IntPtr[] FindWindows(uint processId, string className) {
        var result = new List<IntPtr>();
        EnumWindows((top, ignored) => {
            AddIfMatch(result, top, processId, className);
            EnumChildWindows(top, (child, childIgnored) => {
                AddIfMatch(result, child, processId, className);
                return true;
            }, IntPtr.Zero);
            return true;
        }, IntPtr.Zero);
        return result.ToArray();
    }
}
'@
}

function Wait-Window([int]$ProcessId, [string]$ClassName, [int]$TimeoutSeconds = 5) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $windows = [WidgetStudioSmokeNative]::FindWindows([uint32]$ProcessId, $ClassName)
        if ($windows.Count -gt 0) { return $windows[0] }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "WidgetStudio did not create the expected '$ClassName' window."
}

function Wait-WindowCount(
    [int]$ProcessId, [string]$ClassName, [int]$ExpectedCount, [int]$TimeoutSeconds = 5) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $observed = 0
    do {
        $observed = [WidgetStudioSmokeNative]::FindWindows(
            [uint32]$ProcessId, $ClassName).Count
        if ($observed -eq $ExpectedCount) { return $observed }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Expected $ExpectedCount '$ClassName' windows, observed $observed."
}

function Read-Scene([string]$Path) {
    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Wait-WidgetCount([string]$Path, [int]$MinimumCount, [int]$TimeoutSeconds = 5) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        try {
            $scene = Read-Scene $Path
            if (@($scene.widgets).Count -ge $MinimumCount) { return $scene }
        } catch {}
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "WidgetStudio did not persist at least $MinimumCount widgets."
}

$runtimeDirectory = Split-Path -Parent $ExecutablePath
$process = Start-Process -FilePath $ExecutablePath -WorkingDirectory $runtimeDirectory -PassThru
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

    $scene = Read-Scene $scenePath
    if ($scene.schemaVersion -ne 1 -or @($scene.widgets).Count -lt 1) {
        throw 'WidgetStudio created an invalid or empty first-run scene.'
    }
    if (@($scene.widgets | Where-Object { $_.typeId -eq 'clock' }).Count -lt 1) {
        throw 'WidgetStudio did not create the expected first-run Clock instance.'
    }

    $initialWidgetCount = @($scene.widgets).Count
    $controller = Wait-Window $process.Id 'WidgetStudioHostWindow'
    $initialWidgetWindows = Wait-WindowCount $process.Id `
        'WidgetStudioDesktopWidgetWindow' $initialWidgetCount

    # Open the same registry-driven Widget Library exposed by the tray command.
    [void][WidgetStudioSmokeNative]::SendMessage($controller, 0x0111, [IntPtr]40002, [IntPtr]::Zero)
    $library = Wait-Window $process.Id 'WidgetStudioLibraryWindow'
    $libraryList = [WidgetStudioSmokeNative]::GetDlgItem($library, 100)
    if ($libraryList -eq [IntPtr]::Zero) { throw 'Widget Library did not create its registry list.' }
    $libraryTypeCount = [WidgetStudioSmokeNative]::SendMessage(
        $libraryList, 0x018B, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
    if ($libraryTypeCount -ne 4 -and $libraryTypeCount -ne 5) {
        throw "Widget Library should enumerate four production types and at most one Debug type; observed $libraryTypeCount."
    }
    $productionOffset = $libraryTypeCount - 4

    # Add every non-default production type, then another Clock. Selection
    # indices are the explicit registry order in Release.
    [void][WidgetStudioSmokeNative]::SendMessage(
        $libraryList, 0x0186, [IntPtr]($productionOffset + 1), [IntPtr]::Zero)
    [void][WidgetStudioSmokeNative]::SendMessage($library, 0x0111, [IntPtr]101, [IntPtr]::Zero)
    $scene = Wait-WidgetCount $scenePath ($initialWidgetCount + 1)
    if (@($scene.widgets | Where-Object { $_.typeId -eq 'calendar' }).Count -lt 1) {
        throw 'Widget Library did not create the selected Calendar type.'
    }
    [void][WidgetStudioSmokeNative]::SendMessage(
        $libraryList, 0x0186, [IntPtr]($productionOffset + 2), [IntPtr]::Zero)
    [void][WidgetStudioSmokeNative]::SendMessage($library, 0x0111, [IntPtr]101, [IntPtr]::Zero)
    $scene = Wait-WidgetCount $scenePath ($initialWidgetCount + 2)
    if (@($scene.widgets | Where-Object { $_.typeId -eq 'photo' }).Count -lt 1) {
        throw 'Widget Library did not create the selected Photo type.'
    }
    [void][WidgetStudioSmokeNative]::SendMessage(
        $libraryList, 0x0186, [IntPtr]($productionOffset + 3), [IntPtr]::Zero)
    [void][WidgetStudioSmokeNative]::SendMessage($library, 0x0111, [IntPtr]101, [IntPtr]::Zero)
    $scene = Wait-WidgetCount $scenePath ($initialWidgetCount + 3)
    if (@($scene.widgets | Where-Object { $_.typeId -eq 'music' }).Count -lt 1) {
        throw 'Widget Library did not create the selected Music type.'
    }
    [void][WidgetStudioSmokeNative]::SendMessage(
        $libraryList, 0x0186, [IntPtr]$productionOffset, [IntPtr]::Zero)
    [void][WidgetStudioSmokeNative]::SendMessage($library, 0x0111, [IntPtr]101, [IntPtr]::Zero)
    $scene = Wait-WidgetCount $scenePath ($initialWidgetCount + 4)
    if (@($scene.widgets | Where-Object { $_.typeId -eq 'clock' }).Count -lt 2) {
        throw 'Widget Library did not create a second Clock instance.'
    }

    $widgetWindowsAfterAdd = Wait-WindowCount $process.Id `
        'WidgetStudioDesktopWidgetWindow' @($scene.widgets).Count
    [void][WidgetStudioSmokeNative]::SendMessage($library, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)

    [void][WidgetStudioSmokeNative]::SendMessage($controller, 0x0111, [IntPtr]40003, [IntPtr]::Zero)
    $studio = Wait-Window $process.Id 'WidgetStudioManagementWindow'
    $preview = Wait-Window $process.Id 'WidgetStudioPreviewWindow'
    $applyUniversal = [WidgetStudioSmokeNative]::GetDlgItem($studio, 200)
    if ($applyUniversal -eq [IntPtr]::Zero) { throw 'Widget Studio did not create universal settings controls.' }
    $previewRect = [WidgetStudioSmokeNative+Rect]::new()
    $settingsRect = [WidgetStudioSmokeNative+Rect]::new()
    if (-not [WidgetStudioSmokeNative]::GetWindowRect($preview, [ref]$previewRect) -or
        -not [WidgetStudioSmokeNative]::GetWindowRect($applyUniversal, [ref]$settingsRect) -or
        $settingsRect.Top -lt $previewRect.Bottom) {
        throw 'Widget Studio settings are not laid out below the desktop preview.'
    }
    $previewAspect = [double]($previewRect.Right - $previewRect.Left) /
        [Math]::Max(1, $previewRect.Bottom - $previewRect.Top)
    $monitorAspect = [double][WidgetStudioSmokeNative]::GetSystemMetrics(0) /
        [Math]::Max(1, [WidgetStudioSmokeNative]::GetSystemMetrics(1))
    if ([Math]::Abs($previewAspect - $monitorAspect) -gt 0.04) {
        throw "Widget Studio preview aspect $previewAspect does not match full monitor aspect $monitorAspect."
    }

    $configuredId = @($scene.widgets)[-1].instanceId
    $layoutMode = [WidgetStudioSmokeNative]::GetDlgItem($studio, 209)
    $contentScale = [WidgetStudioSmokeNative]::GetDlgItem($studio, 211)
    $appearanceMode = [WidgetStudioSmokeNative]::GetDlgItem($studio, 212)
    $glass = [WidgetStudioSmokeNative]::GetDlgItem($studio, 213)
    $opacity = [WidgetStudioSmokeNative]::GetDlgItem($studio, 214)
    $blur = [WidgetStudioSmokeNative]::GetDlgItem($studio, 215)
    $radius = [WidgetStudioSmokeNative]::GetDlgItem($studio, 216)
    $positionA = [WidgetStudioSmokeNative]::GetDlgItem($studio, 217)
    $positionB = [WidgetStudioSmokeNative]::GetDlgItem($studio, 218)
    $sizeA = [WidgetStudioSmokeNative]::GetDlgItem($studio, 219)
    $sizeB = [WidgetStudioSmokeNative]::GetDlgItem($studio, 220)
    $widgetCheck = [WidgetStudioSmokeNative]::GetDlgItem($studio, 400)
    $layoutPageButton = [WidgetStudioSmokeNative]::GetDlgItem($studio, 229)
    $widgetPageButton = [WidgetStudioSmokeNative]::GetDlgItem($studio, 231)
    $applyUniversalButton = [WidgetStudioSmokeNative]::GetDlgItem($studio, 200)
    foreach ($control in @($layoutMode, $contentScale, $appearanceMode, $glass, $opacity,
            $blur, $radius, $positionA, $positionB, $sizeA, $sizeB, $widgetCheck,
            $applyUniversalButton, $layoutPageButton, $widgetPageButton)) {
        if ($control -eq [IntPtr]::Zero) { throw 'Widget Studio is missing a stable settings control.' }
    }
    foreach ($settingId in 400..405) {
        if ([WidgetStudioSmokeNative]::GetDlgItem($studio, $settingId) -eq [IntPtr]::Zero) {
            throw "Widget Studio does not expose Clock setting control $settingId directly."
        }
    }
    $studioBounds = [WidgetStudioSmokeNative+Rect]::new()
    $navigationBeforeWheel = [WidgetStudioSmokeNative+Rect]::new()
    $navigationAfterWheel = [WidgetStudioSmokeNative+Rect]::new()
    [void][WidgetStudioSmokeNative]::GetWindowRect($studio, [ref]$studioBounds)
    [void][WidgetStudioSmokeNative]::GetWindowRect($layoutPageButton, [ref]$navigationBeforeWheel)
    [void][WidgetStudioSmokeNative]::SendMessage(
        $studio, 0x0115, [IntPtr]3, [IntPtr]::Zero) # Ignored WM_VSCROLL / SB_PAGEDOWN
    Start-Sleep -Milliseconds 50
    [void][WidgetStudioSmokeNative]::GetWindowRect($layoutPageButton, [ref]$navigationAfterWheel)
    if ($navigationAfterWheel.Top -ne $navigationBeforeWheel.Top) {
        throw 'Widget Studio unexpectedly moved controls in response to a scroll message.'
    }
    foreach ($control in @($layoutMode, $contentScale, $positionA, $positionB, $sizeA,
            $sizeB, $applyUniversalButton, $layoutPageButton, $widgetPageButton)) {
        $controlBounds = [WidgetStudioSmokeNative+Rect]::new()
        if ([WidgetStudioSmokeNative]::IsWindowVisible($control) -and
            [WidgetStudioSmokeNative]::GetWindowRect($control, [ref]$controlBounds) -and
            ($controlBounds.Top -lt $studioBounds.Top -or $controlBounds.Bottom -gt $studioBounds.Bottom)) {
            throw 'Widget Studio exposed a settings control outside its non-scrolling window.'
        }
    }
    [void][WidgetStudioSmokeNative]::SendMessage(
        $widgetPageButton, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 50
    if (-not [WidgetStudioSmokeNative]::IsWindowVisible($widgetCheck) -or
        [WidgetStudioSmokeNative]::IsWindowVisible($applyUniversalButton)) {
        throw 'Widget Studio page navigation did not expose Widget content cleanly.'
    }
    [void][WidgetStudioSmokeNative]::SendMessage($layoutMode, 0x014E, [IntPtr]1, [IntPtr]::Zero)
    $layoutChanged = 209 -bor (1 -shl 16)
    [void][WidgetStudioSmokeNative]::SendMessage($studio, 0x0111, [IntPtr]$layoutChanged, $layoutMode)
    [void][WidgetStudioSmokeNative]::SendMessage($appearanceMode, 0x014E, [IntPtr]1, [IntPtr]::Zero)
    [void][WidgetStudioSmokeNative]::SendMessage($glass, 0x014E, [IntPtr]2, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 100
    [void][WidgetStudioSmokeNative]::SendMessage(
        $applyUniversalButton, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    $scene = Read-Scene $scenePath
    $configured = @($scene.widgets | Where-Object { $_.instanceId -eq $configuredId })[0]
    if (-not $configured -or $configured.layoutMode -ne 'free' -or
        [double]$configured.free.width -le 100.0 -or [double]$configured.free.height -le 100.0 -or
        $configured.appearance.mode -ne 'light' -or $configured.appearance.surface -ne 'solid' -or
        $configured.appearance.glass -ne $false) {
        throw 'Widget Studio did not persist the converted free layout and appearance toggles.'
    }

    [void][WidgetStudioSmokeNative]::SendMessage($widgetCheck, 0x00F1, [IntPtr]0, [IntPtr]::Zero)
    [void][WidgetStudioSmokeNative]::SendMessage(
        $studio, 0x0111, [IntPtr](400 -bor (0 -shl 16)), $widgetCheck)
    $scene = Read-Scene $scenePath
    $configured = @($scene.widgets | Where-Object { $_.instanceId -eq $configuredId })[0]
    if ($configured.state.use24Hour -ne 'false') {
        throw 'Widget Studio did not apply the selected Clock-specific boolean setting.'
    }

    [void][WidgetStudioSmokeNative]::SendMessage($studio, 0x0111, [IntPtr]206, [IntPtr]::Zero)
    $scene = Wait-WidgetCount $scenePath ($initialWidgetCount + 5)
    $duplicatedCount = @($scene.widgets).Count
    $duplicate = @($scene.widgets)[-1]
    if ($duplicate.instanceId -eq $configuredId -or $duplicate.typeId -ne 'clock' -or
        $duplicate.layoutMode -ne 'free' -or $duplicate.state.use24Hour -ne 'false' -or
        $duplicate.appearance.mode -ne 'light' -or $duplicate.appearance.surface -ne 'solid' -or
        $duplicate.appearance.glass -ne $false) {
        throw 'Widget Studio Duplicate did not preserve common and widget-specific state.'
    }
    [void][WidgetStudioSmokeNative]::SendMessage($studio, 0x0111, [IntPtr]207, [IntPtr]::Zero)
    $removeDeadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        $scene = Read-Scene $scenePath
        if (@($scene.widgets).Count -eq $duplicatedCount - 1) { break }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $removeDeadline)
    if (@($scene.widgets).Count -ne $duplicatedCount - 1) {
        throw 'Widget Studio Remove did not update the persisted scene.'
    }
    [void][WidgetStudioSmokeNative]::SendMessage($studio, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)

    [void][WidgetStudioSmokeNative]::SendMessage($controller, 0x0111, [IntPtr]40006, [IntPtr]::Zero)
    $scene = Read-Scene $scenePath
    if (@($scene.widgets | Where-Object { -not $_.locked }).Count -ne 0) {
        throw 'The Lock All command did not persist generic lock state for every widget.'
    }

    $secondProcess = Start-Process -FilePath $ExecutablePath -WorkingDirectory $runtimeDirectory -PassThru
    try {
        if (-not $secondProcess.WaitForExit(5000)) {
            throw 'A second WidgetStudio launch remained running instead of honoring the single-instance guard.'
        }
        if ($secondProcess.ExitCode -ne 2) {
            throw "The guarded second WidgetStudio launch should exit with diagnostic code 2; observed $($secondProcess.ExitCode)."
        }
        $process.Refresh()
        if ($process.HasExited) {
            throw 'The original WidgetStudio process exited during the single-instance check.'
        }
    } finally {
        if (-not $secondProcess.HasExited) {
            Stop-Process -Id $secondProcess.Id -Force
            $secondProcess.WaitForExit()
        }
    }

    # Exit through the application's own command, relaunch, and verify that the
    # persisted scene recreates every per-widget HWND.
    $persistedWidgetCount = @($scene.widgets).Count
    [void][WidgetStudioSmokeNative]::SendMessage($controller, 0x0111, [IntPtr]40004, [IntPtr]::Zero)
    if (-not $process.WaitForExit(5000)) {
        throw 'WidgetStudio did not exit through its normal Exit command.'
    }
    $process = Start-Process -FilePath $ExecutablePath -WorkingDirectory $runtimeDirectory -PassThru
    $controller = Wait-Window $process.Id 'WidgetStudioHostWindow'
    $restoredWidgetWindows = Wait-WindowCount $process.Id `
        'WidgetStudioDesktopWidgetWindow' $persistedWidgetCount
    $restoredScene = Read-Scene $scenePath
    if ($restoredWidgetWindows -ne $persistedWidgetCount -or
        @($restoredScene.widgets).Count -ne $persistedWidgetCount) {
        throw 'WidgetStudio did not restore every persisted widget after restart.'
    }
    $restoredWindows = [WidgetStudioSmokeNative]::FindWindows(
        [uint32]$process.Id, 'WidgetStudioDesktopWidgetWindow')
    $parentClasses = @()
    foreach ($widgetWindow in $restoredWindows) {
        $parent = [WidgetStudioSmokeNative]::GetParent($widgetWindow)
        $parentClasses += if ($parent -eq [IntPtr]::Zero) {
            'top-level'
        } else {
            [WidgetStudioSmokeNative]::ClassNameOf($parent)
        }
        $windowRect = [WidgetStudioSmokeNative+Rect]::new()
        if (-not [WidgetStudioSmokeNative]::GetWindowRect($widgetWindow, [ref]$windowRect)) {
            throw 'Could not inspect a restored desktop widget window.'
        }
        $hitX = $windowRect.Left + 1
        $hitY = $windowRect.Top + 1
        $packedPoint = (($hitY -band 0xFFFF) -shl 16) -bor ($hitX -band 0xFFFF)
        $hit = [WidgetStudioSmokeNative]::SendMessage(
            $widgetWindow, 0x0084, [IntPtr]::Zero, [IntPtr]$packedPoint).ToInt64()
        if ($hit -ne -1) { throw 'A passive widget margin did not return HTTRANSPARENT.' }
    }
    $desktopBackend = if (@($parentClasses | Where-Object { $_ -eq 'WorkerW' }).Count -eq
            $restoredWindows.Count) { 'WorkerW' } elseif (@($parentClasses | Where-Object {
            $_ -eq 'top-level' }).Count -eq $restoredWindows.Count) { 'windowed-fallback' } else { 'mixed' }

    $report = [ordered]@{
        passed = $true
        processId = $process.Id
        secondLaunchExited = $true
        secondLaunchExitCode = $secondProcess.ExitCode
        controllerArchitecture = 'hidden-controller-with-per-widget-HWNDs'
        portableScenePath = $scenePath
        schemaVersion = $scene.schemaVersion
        widgetCount = @($scene.widgets).Count
        initialWidgetWindowCount = $initialWidgetWindows
        libraryTypeCount = $libraryTypeCount
        debugLibraryEntryPresent = $productionOffset -eq 1
        widgetWindowCountAfterAdd = $widgetWindowsAfterAdd
        registryCreatePassed = $true
        allProductionWidgetTypesCreated = $true
        multipleInstancePassed = $true
        passiveHitTestingPassed = $true
        studioPreviewAboveSettings = $true
        studioPreviewUsesFullMonitorAspect = $true
        settingsFitWithoutScrolling = $true
        universalSettingsPassed = $true
        widgetSpecificSettingsPassed = $true
        duplicateStatePassed = $true
        studioDuplicateRemovePassed = $true
        lockAllPassed = $true
        restartRestorePassed = $true
        restoredWidgetWindowCount = $restoredWidgetWindows
        desktopBackend = $desktopBackend
        observedAtUtc = [DateTime]::UtcNow.ToString('o')
    }
    $report | ConvertTo-Json | Set-Content -LiteralPath $ReportPath -Encoding UTF8
} finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
}
