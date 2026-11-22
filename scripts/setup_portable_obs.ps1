# Configure two portable OBS instances for Wormhole only (no Teleport).
# Copyright (C) 2026 Wormhole contributors
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Usage: powershell -File scripts/setup_portable_obs.ps1
# Requires third_party/obs-portable/{sender,receiver} (see HARNESS.md).

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$base = Join-Path $root 'third_party\obs-portable'
$sender = Join-Path $base 'sender'
$receiver = Join-Path $base 'receiver'
$template = Join-Path $sender 'config\obs-studio\basic\scenes\Untitled.json'

foreach ($dir in @($sender, $receiver)) {
    if (-not (Test-Path (Join-Path $dir 'bin\64bit\obs64.exe'))) {
        throw "Portable OBS missing at $dir"
    }
    # Teleport must not be present in these instances.
    $tp = Join-Path $dir 'obs-plugins\64bit\obs-teleport.dll'
    if (Test-Path $tp) { Remove-Item $tp -Force }
    $tpc = Join-Path $dir 'config\obs-studio\plugin_config\obs-teleport'
    if (Test-Path $tpc) { Remove-Item $tpc -Recurse -Force }

    New-Item -ItemType Directory -Force -Path `
        (Join-Path $dir 'config\obs-studio\basic\scenes'),
        (Join-Path $dir 'config\obs-studio\basic\profiles\Wormhole') | Out-Null
    Set-Content (Join-Path $dir 'portable_mode.txt') '' -Encoding UTF8
    $ini = Join-Path $dir 'config\obs-studio\basic\profiles\Wormhole\basic.ini'
    @'
[General]
Name=Wormhole

[Video]
BaseCX=1280
BaseCY=720
OutputCX=1280
OutputCY=720
FPSType=0
FPSCommon=30
FPSInt=30
FPSNum=30
FPSDen=1

[Audio]
SampleRate=48000
ChannelSetup=2
DesktopDevice1=disabled
DesktopDevice2=disabled
AuxDevice1=disabled
AuxDevice2=disabled

[Websocket]
ServerEnabled=true
ServerPort=4455
ServerPassword=
'@ | Set-Content $ini -Encoding UTF8
    $sentinel = Join-Path $dir 'config\obs-studio\.sentinel'
    if (Test-Path $sentinel) { Remove-Item $sentinel -Recurse -Force }
}
$rxIni = Join-Path $receiver 'config\obs-studio\basic\profiles\Wormhole\basic.ini'
$rxText = Get-Content $rxIni -Raw
$rxText = [regex]::Replace($rxText, 'ServerPort=4455', 'ServerPort=4456')
[IO.File]::WriteAllText($rxIni, $rxText, (New-Object Text.UTF8Encoding $false))

function New-Source([string]$name, [string]$id, $settings, [int]$mixers) {
    return [ordered]@{
        prev_ver = 553648128
        name = $name
        uuid = [guid]::NewGuid().ToString()
        id = $id
        versioned_id = $id
        settings = $settings
        mixers = $mixers
        sync = 0
        flags = 0
        volume = 1.0
        balance = 0.5
        enabled = $true
        muted = $false
        'push-to-mute' = $false
        'push-to-mute-delay' = 0
        'push-to-talk' = $false
        'push-to-talk-delay' = 0
        hotkeys = [ordered]@{}
        deinterlace_mode = 0
        deinterlace_field_order = 0
        monitoring_type = 0
        monitoring_enabled = $false
        private_settings = [ordered]@{}
    }
}

function Add-Item($sceneSource, $source) {
    $sceneSource.settings.items += [ordered]@{
        name = $source.name
        uuid = [guid]::NewGuid().ToString()
        id = $sceneSource.settings.items.Count + 1
        source_uuid = $source.uuid
        visible = $true
        locked = $false
        rot = 0.0
        pos = [ordered]@{ x = 0.0; y = 0.0 }
        scale = [ordered]@{ x = 1.0; y = 1.0 }
        align = 5
        bounds = [ordered]@{ x = 0.0; y = 0.0; type = 0; align = 5 }
    }
}

function Remove-GlobalAudioDevices($scene) {
    foreach ($prop in @($scene.PSObject.Properties.Name)) {
        if ($prop -match 'AudioDevice') { $scene.PSObject.Properties.Remove($prop) | Out-Null }
    }
}

# Gaming PC: main monitor + one audio output. Default devices are disabled so
# the test scene contains only these sources.
$senderScene = Get-Content $template -Raw | ConvertFrom-Json
Remove-GlobalAudioDevices $senderScene
$sceneSource = $senderScene.sources | Where-Object { $_.id -eq 'scene' } | Select-Object -First 1
$senderScene.sources = @($sceneSource)
$sceneSource.settings.items = @()
$sceneSource.settings.id_counter = 0
$display = New-Source 'Main Monitor' 'monitor_capture' ([ordered]@{ monitor = 0; capture_cursor = $true }) 0
$senderScene.sources += $display
Add-Item $sceneSource $display
$audio1 = New-Source 'Desktop Audio 2' 'wasapi_output_capture' ([ordered]@{ device_id = 'default' }) 255
$senderScene.sources += $audio1
Add-Item $sceneSource $audio1
$senderScene.name = 'WormholeSender'
$senderScene | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $sender 'config\obs-studio\basic\scenes\WormholeSender.json') -Encoding UTF8

# Streaming PC: video capture + one audio channel from the gaming PC.
$receiverScene = Get-Content $template -Raw | ConvertFrom-Json
Remove-GlobalAudioDevices $receiverScene
$sceneSource = $receiverScene.sources | Where-Object { $_.id -eq 'scene' } | Select-Object -First 1
$receiverScene.sources = @($sceneSource)
$sceneSource.settings.items = @()
$sceneSource.settings.id_counter = 0
$rx = New-Source 'Wormhole Video Capture' 'wormhole-source' ([ordered]@{ host = '127.0.0.1'; port = 18090 }) 0
$receiverScene.sources += $rx
Add-Item $sceneSource $rx
$rxAudio = New-Source 'Wormhole Audio' 'wormhole-audio' ([ordered]@{ host = '127.0.0.1'; port = 18091 }) 255
$receiverScene.sources += $rxAudio
Add-Item $sceneSource $rxAudio
$receiverScene.name = 'WormholeReceiver'
$receiverScene | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $receiver 'config\obs-studio\basic\scenes\WormholeReceiver.json') -Encoding UTF8

# Wormhole sender settings. No "Audio Tracks" option: Wormhole follows OBS.
$whDir = Join-Path $sender 'config\obs-studio\plugin_config\wormhole'
New-Item -ItemType Directory -Force -Path $whDir | Out-Null
[IO.File]::WriteAllText((Join-Path $whDir 'wormhole.json'),
    '{"enabled":true,"mode":"server","name":"GAMING-PC-NEO","port":18090,"quality":90,"tracks":2}',
    (New-Object Text.UTF8Encoding $false))
# Receiver settings stay disabled: its source is in the scene, not an output.
$whRxDir = Join-Path $receiver 'config\obs-studio\plugin_config\wormhole'
New-Item -ItemType Directory -Force -Path $whRxDir | Out-Null
[IO.File]::WriteAllText((Join-Path $whRxDir 'wormhole.json'),
    '{"enabled":false,"mode":"client","port":18090,"quality":90,"client_host":"127.0.0.1","client_port":18090}',
    (New-Object Text.UTF8Encoding $false))

# Replace bundled modules that abort a clean OBS 33 launch with a no-op module.
$stubSource = Join-Path $root 'scripts\obs_plugin_stub.c'
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vsWhere)) { throw "Visual Studio vswhere.exe not found" }
$vsInstall = & $vsWhere -latest -products * -property installationPath | Select-Object -First 1
$vcvars = Join-Path $vsInstall 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found at $vcvars" }
New-Item -ItemType Directory -Force -Path (Join-Path $root 'build\c') | Out-Null
$stubDll = Join-Path $root 'build\c\obs_plugin_stub.dll'
$buildCmd = "`"$vcvars`" && cl /nologo /LD /W4 /O2 /Fo:build\c\obs_plugin_stub.obj /Fe:build\c\obs_plugin_stub.dll `"$stubSource`""
cmd.exe /c $buildCmd | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $stubDll)) { throw "Failed to build OBS module stub" }
$failingModules = @(
    'core\aja\aja.dll',
    'core\aja-output-ui\aja-output-ui.dll',
    'core\decklink\decklink.dll',
    'core\decklink-captions\decklink-captions.dll',
    'core\decklink-output-ui\decklink-output-ui.dll',
    'obs-plugins\64bit\multi-source-record.dll',
    'obs-plugins\64bit\obs-shaderfilter.dll'
)
foreach ($dir in @($sender, $receiver)) {
    foreach ($module in $failingModules) {
        $target = Join-Path $dir $module
        if (Test-Path $target) { Copy-Item $stubDll $target -Force }
    }
}
Write-Host "Configured Wormhole-only sender and receiver"
