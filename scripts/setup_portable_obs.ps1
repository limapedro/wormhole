# Wormhole - configure two portable OBS instances for Teleport testing.
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
    New-Item -ItemType Directory -Force -Path (Join-Path $dir 'config\obs-studio\basic\scenes'),
        (Join-Path $dir 'config\obs-studio\basic\profiles\Wormhole'),
        (Join-Path $dir 'config\obs-studio\plugin_config\obs-teleport') | Out-Null
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
'@ | Set-Content $ini -Encoding UTF8
}

# Sender: enable the Teleport output on a fixed TCP port, and add a colour
# source so video frames are non-trivial.
$teleportSender = Join-Path $sender 'config\obs-studio\plugin_config\obs-teleport\obs-teleport.json'
@'
{
    "teleport-enabled": true,
    "identifier": "WormholeSender",
    "quality": 90,
    "advanced": true,
    "port": 18081
}
'@ | Set-Content $teleportSender -Encoding UTF8

$senderScene = Get-Content $template -Raw | ConvertFrom-Json
$colourUuid = [guid]::NewGuid().ToString()
$colour = [ordered]@{
    prev_ver = 553648128
    name = 'Colour Bars'
    uuid = $colourUuid
    id = 'color_source'
    versioned_id = 'color_source'
    settings = [ordered]@{ color = 4294901760; width = 1280; height = 720 }
    mixers = 0
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
$senderScene.sources += $colour
$sceneSource = $senderScene.sources | Where-Object { $_.id -eq 'scene' } | Select-Object -First 1
$sceneSource.settings.id_counter = 1
$sceneSource.settings.items += [ordered]@{
    name = 'Colour Bars'
    uuid = [guid]::NewGuid().ToString()
    id = 1
    source_uuid = $colourUuid
    visible = $true
    locked = $false
    rot = 0.0
    pos = [ordered]@{ x = 0.0; y = 0.0 }
    scale = [ordered]@{ x = 1.0; y = 1.0 }
    align = 5
    bounds = [ordered]@{ x = 0.0; y = 0.0; type = 0; align = 5 }
}
$senderScene.name = 'WormholeSender'
$senderScene | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $sender 'config\obs-studio\basic\scenes\WormholeSender.json') -Encoding UTF8

# Receiver: one Teleport source connected manually through the harness proxy.
$receiverScene = Get-Content $template -Raw | ConvertFrom-Json
$teleportUuid = [guid]::NewGuid().ToString()
$teleportSource = [ordered]@{
    prev_ver = 553648128
    name = 'Teleport'
    uuid = $teleportUuid
    id = 'teleport-source'
    versioned_id = 'teleport-source'
    settings = [ordered]@{
        teleport_list = ''
        'manual-ip' = '127.0.0.1:18080'
        'has-audio-and-video' = $true
        advanced = $true
    }
    mixers = 255
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
$receiverScene.sources += $teleportSource
$sceneSource = $receiverScene.sources | Where-Object { $_.id -eq 'scene' } | Select-Object -First 1
$sceneSource.settings.id_counter = 1
$sceneSource.settings.items += [ordered]@{
    name = 'Teleport'
    uuid = [guid]::NewGuid().ToString()
    id = 1
    source_uuid = $teleportUuid
    visible = $true
    locked = $false
    rot = 0.0
    pos = [ordered]@{ x = 0.0; y = 0.0 }
    scale = [ordered]@{ x = 1.0; y = 1.0 }
    align = 5
    bounds = [ordered]@{ x = 0.0; y = 0.0; type = 0; align = 5 }
}
$receiverScene.name = 'WormholeReceiver'
$receiverScene | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $receiver 'config\obs-studio\basic\scenes\WormholeReceiver.json') -Encoding UTF8

Write-Host "Configured sender: $sender"
Write-Host "Configured receiver: $receiver"
