# Wormhole - launch two portable OBS instances through the harness proxy,
# sample CPU/memory, and leave a raw capture for the dissector.
# Copyright (C) 2026 Wormhole contributors
# SPDX-License-Identifier: GPL-2.0-or-later
param(
    [double]$WindowSeconds = 6,
    [string]$Config = 'harness/config/obs-proxy.toml',
    [string]$Harness = 'target\debug\wormhole-harness.exe'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$rxExe = 'third_party\obs-portable\receiver\bin\64bit\obs64.exe'
$txExe = 'third_party\obs-portable\sender\bin\64bit\obs64.exe'
$rxCwd = 'third_party\obs-portable\receiver\bin\64bit'
$txCwd = 'third_party\obs-portable\sender\bin\64bit'
$results = 'exp_results'
New-Item -ItemType Directory -Force -Path $results | Out-Null
Remove-Item (Join-Path $results 'obs-capture.bin'), (Join-Path $results 'obs-upstream.bin') -ErrorAction SilentlyContinue

$proxy = $null; $rx = $null; $tx = $null
try {
    $proxy = Start-Process -FilePath $Harness -ArgumentList '--config', $Config, 'proxy' -PassThru `
        -WindowStyle Hidden -RedirectStandardOutput "$results\obs-proxy.out" -RedirectStandardError "$results\obs-proxy.err"
    Start-Sleep -Milliseconds 800
    $rx = Start-Process -FilePath $rxExe -ArgumentList '--portable','--multi','--profile','Wormhole','--collection','WormholeReceiver','--scene','Scene' `
        -WorkingDirectory $rxCwd -PassThru -WindowStyle Hidden -RedirectStandardOutput "$results\obs-rx.out" -RedirectStandardError "$results\obs-rx.err"
    Start-Sleep -Seconds 2
    $tx = Start-Process -FilePath $txExe -ArgumentList '--portable','--multi','--profile','Wormhole','--collection','WormholeSender','--scene','Scene' `
        -WorkingDirectory $txCwd -PassThru -WindowStyle Hidden -RedirectStandardOutput "$results\obs-tx.out" -RedirectStandardError "$results\obs-tx.err"
    Start-Sleep -Seconds 4

    $r1 = (Get-Process -Id $rx.Id -ErrorAction SilentlyContinue).CPU
    $t1 = (Get-Process -Id $tx.Id -ErrorAction SilentlyContinue).CPU
    $begin = Get-Date
    Start-Sleep -Seconds ([int][math]::Ceiling($WindowSeconds))
    $r2 = (Get-Process -Id $rx.Id -ErrorAction SilentlyContinue).CPU
    $t2 = (Get-Process -Id $tx.Id -ErrorAction SilentlyContinue).CPU
    $seconds = ((Get-Date) - $begin).TotalSeconds
    $logical = [Environment]::ProcessorCount

    $rxDelta = $r2 - $r1
    $txDelta = $t2 - $t1
    $rxPct = 100.0 * $rxDelta / $seconds / $logical
    $txPct = 100.0 * $txDelta / $seconds / $logical
    $rxWs = (Get-Process -Id $rx.Id).WorkingSet64 / 1MB
    $txWs = (Get-Process -Id $tx.Id).WorkingSet64 / 1MB

    "receiver_cpu_delta_s=$([math]::Round($rxDelta,3))"
    "sender_cpu_delta_s=$([math]::Round($txDelta,3))"
    "window_s=$([math]::Round($seconds,2)) logical_cpus=$logical"
    "receiver_machine_pct=$([math]::Round($rxPct,2))"
    "sender_machine_pct=$([math]::Round($txPct,2))"
    "receiver_working_set_MB=$([math]::Round($rxWs,1))"
    "sender_working_set_MB=$([math]::Round($txWs,1))"

    $csv = Join-Path $results 'two-obs-cpu.csv'
    if (-not (Test-Path $csv)) {
        'timestamp,receiver_cpu_delta_s,sender_cpu_delta_s,window_s,logical_cpus,receiver_pct,sender_pct,receiver_ws_MB,sender_ws_MB' | Set-Content $csv
    }
    "$(Get-Date -Format o),$([math]::Round($rxDelta,3)),$([math]::Round($txDelta,3)),$([math]::Round($seconds,2)),$logical,$([math]::Round($rxPct,2)),$([math]::Round($txPct,2)),$([math]::Round($rxWs,1)),$([math]::Round($txWs,1))" | Add-Content $csv
}
finally {
    foreach ($p in @($rx, $tx)) {
        if ($p -and -not $p.HasExited) { try { $p.CloseMainWindow() | Out-Null } catch {} }
    }
    Start-Sleep -Seconds 2
    foreach ($p in @($rx, $tx, $proxy)) {
        if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
    }
}
Get-ChildItem (Join-Path $results 'obs-capture.bin'), (Join-Path $results 'obs-upstream.bin') -ErrorAction SilentlyContinue | Select-Object Name, Length
