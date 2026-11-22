# Automated Wormhole audio-channel test.
# Launches both portable OBS instances, discovers output devices, streams two
# channels, switches a sender source mid-test, and records ffmpeg levels.
# Evidence: exp_results/audio_channel_test.json
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$mcp = Join-Path (Split-Path $root) 'OBS-MCP'

Get-Process obs64 -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
& (Join-Path $root 'scripts\setup_portable_obs.ps1') | Out-Null
Set-Location $mcp
Copy-Item (Join-Path $root 'build\plugin\wormhole.dll') `
    (Join-Path $root 'third_party\obs-portable\sender\obs-plugins\64bit\wormhole.dll') -Force
Copy-Item (Join-Path $root 'build\plugin\wormhole.dll') `
    (Join-Path $root 'third_party\obs-portable\receiver\obs-plugins\64bit\wormhole.dll') -Force

$script:id = 0
$requests = New-Object System.Collections.ArrayList
function Add-Req([string]$method, $argObj) {
    $script:id++
    $o = [ordered]@{ jsonrpc = '2.0'; id = $script:id; method = $method }
    if ($argObj) { $o.params = $argObj }
    [void]$script:requests.Add(($o | ConvertTo-Json -Compress -Depth 20))
}
function Add-Tool([string]$name, $argObj) { Add-Req 'tools/call' ([ordered]@{ name = $name; arguments = $argObj }) }
function Add-Ws([string]$instance, [string]$type, $data, [int]$rid = 1) {
    $d = [ordered]@{ requestType = $type; requestId = $rid }
    if ($data) { $d.requestData = $data }
    $wire = [ordered]@{ op = 6; d = $d } | ConvertTo-Json -Compress -Depth 20
    Add-Tool 'obs_ws_call' ([ordered]@{ name = $instance; request = $wire })
}
function Add-Wait([int]$ms) { Add-Tool 'wait_ms' ([ordered]@{ ms = $ms }) }

$out = Join-Path $root 'exp_results\audio_channel_test.jsonl'
Set-Content $out '' -Encoding UTF8
$tag = Get-Date -Format 'HHmmss'
$ffmpeg = 'C:\ffmpeg\ffmpeg-master-latest-win64-gpl\bin\ffmpeg.exe'
$ffprobe = 'C:\ffmpeg\ffmpeg-master-latest-win64-gpl\bin\ffprobe.exe'
$videos = 'C:\Users\multi\Videos'

function Measure-Recording([string]$path) {
    if (-not (Test-Path $path)) { return @{ error = 'missing' } }
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $text = & $ffmpeg -hide_banner -i $path -vn -af volumedetect -f null NUL 2>&1 | Out-String
    $ErrorActionPreference = $old
    $mean = [regex]::Match($text, 'mean_volume:\s*(-?[\d.]+) dB').Groups[1].Value
    $max = [regex]::Match($text, 'max_volume:\s*(-?[\d.]+) dB').Groups[1].Value
    return @{ mean_db = $mean; max_db = $max }
}

# Phase 1: launch and discover devices.
Add-Tool 'obs_launch' ([ordered]@{ name = 'sender' })
Add-Wait 7000
Add-Ws 'sender' 'GetInputPropertiesListPropertyItems' `
    ([ordered]@{ inputName = 'Desktop Audio 2'; propertyName = 'device_id' }) 1
$requests | & 'build\obs-mcp.exe' --config 'config/gaming-streaming.toml' | Set-Content $out -Encoding UTF8
$requests.Clear(); $script:id = 0

$devices = @()
foreach ($line in (Get-Content $out)) {
    $o = $line | ConvertFrom-Json
    if (-not $o.result) { continue }
    $t = $o.result.content[0].text
    if ($t -match '^\{') {
        try {
            $inner = $t | ConvertFrom-Json
            if ($inner.d.requestId -eq 1) { $devices = @($inner.d.responseData.propertyItems) }
        } catch {}
    }
}
# Prefer Sonar Media/Aux; fall back to first two non-default entries.
$candidates = @($devices | Where-Object {
    $_.itemValue -and $_.itemValue -ne 'default' -and
    ($_.itemName -match 'Media|Aux|Gaming|Microphone')
})
if ($candidates.Count -lt 2) { $candidates = @($devices | Where-Object { $_.itemValue -and $_.itemValue -ne 'default' }) }
$devA = $candidates[0]
$devB = if ($candidates.Count -gt 1) { $candidates[1] } else { $candidates[0] }
Write-Host "devices: A=$($devA.itemName) B=$($devB.itemName)"

# Phase 2: add two audio sources on the gaming PC and start the receiver.
Add-Ws 'sender' 'CreateInput' ([ordered]@{ sceneName = 'Scene'; inputName = ('Test Media ' + $tag); inputKind = 'wasapi_output_capture'; inputSettings = [ordered]@{ device_id = $devA.itemValue } }) 1
Add-Ws 'sender' 'CreateInput' ([ordered]@{ sceneName = 'Scene'; inputName = ('Test Aux ' + $tag); inputKind = 'wasapi_output_capture'; inputSettings = [ordered]@{ device_id = $devB.itemValue } }) 2
Add-Wait 4000
Add-Tool 'obs_launch' ([ordered]@{ name = 'receiver' })
Add-Wait 9000
Add-Ws 'receiver' 'GetInputPropertiesListPropertyItems' ([ordered]@{ inputName = 'Wormhole Audio'; propertyName = 'channel' }) 3
Add-Ws 'receiver' 'CreateInput' ([ordered]@{ sceneName = 'Scene'; inputName = ('Test Network Audio ' + $tag); inputKind = 'wormhole-audio'; inputSettings = [ordered]@{ host = '127.0.0.1'; port = 18093; channel = '127.0.0.1|18093' } }) 4
Add-Wait 3000

# Phase 3: record channel A, switch sender target, record channel B.
Add-Ws 'receiver' 'StartRecord' $null 5
Add-Wait 7000
Add-Ws 'receiver' 'StopRecord' $null 6
Add-Wait 2000
$requests | & 'build\obs-mcp.exe' --config 'config/gaming-streaming.toml' | Add-Content $out -Encoding UTF8

Add-Ws 'sender' 'SetInputSettings' ([ordered]@{ inputName = ('Wormhole Audio - Test Media ' + $tag); inputSettings = [ordered]@{ target = ('Test Aux ' + $tag); connected = $true }; overlay = $true }) 7
Add-Wait 3000
Add-Ws 'receiver' 'StartRecord' $null 8
Add-Wait 7000
Add-Ws 'receiver' 'StopRecord' $null 9
Add-Wait 2000
$requests | & 'build\obs-mcp.exe' --config 'config/gaming-streaming.toml' | Add-Content $out -Encoding UTF8

# Collect output paths and measure.
$paths = @()
foreach ($line in (Get-Content $out)) {
    $o = $line | ConvertFrom-Json
    if (-not $o.result) { continue }
    $t = $o.result.content[0].text
    if ($t -match '^\{') {
        try {
            $inner = $t | ConvertFrom-Json
            if ($inner.d.requestType -eq 'StopRecord' -and $inner.d.responseData.outputPath) {
                $paths += $inner.d.responseData.outputPath.Replace('/', '\')
            }
        } catch {}
    }
}
$levels = @()
foreach ($p in $paths) { $levels += (Measure-Recording $p) }

$result = [ordered]@{
    date = (Get-Date).ToString('o')
    deviceA = [ordered]@{ name = $devA.itemName; id = $devA.itemValue }
    deviceB = [ordered]@{ name = $devB.itemName; id = $devB.itemValue }
    recordings = $paths
    levels = $levels
}
$result | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $root 'exp_results\audio_channel_test.json') -Encoding UTF8
$result | ConvertTo-Json -Depth 8
