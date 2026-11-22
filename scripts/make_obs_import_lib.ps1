$ErrorActionPreference = 'Stop'
$vs = 'C:\Program Files\Microsoft Visual Studio\18\Community'
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$dumpbin = Get-ChildItem (Join-Path $vs 'VC\Tools\MSVC') -Recurse -Filter dumpbin.exe |
    Where-Object { $_.FullName -match 'bin\\Hostx64\\x64' } | Select-Object -First 1
$libexe = Get-ChildItem (Join-Path $vs 'VC\Tools\MSVC') -Recurse -Filter lib.exe |
    Where-Object { $_.FullName -match 'bin\\Hostx64\\x64' } | Select-Object -First 1
$outdir = 'third_party/obs-sdk/lib'
New-Item -ItemType Directory -Force -Path $outdir | Out-Null

$cmd = "`"$vcvars`" && `"$($dumpbin.FullName)`" /exports `"C:\Program Files\obs-studio\bin\64bit\obs.dll`""
$exports = & cmd.exe /c $cmd
$names = $exports | ForEach-Object {
    if ($_ -match '^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)') { $Matches[1] }
} | Sort-Object -Unique
"exports=$($names.Count)"
$def = @('LIBRARY obs', 'EXPORTS') + ($names | ForEach-Object { "    $_" })
[IO.File]::WriteAllLines((Join-Path $outdir 'obs.def'), $def)
$cmd2 = "`"$vcvars`" && `"$($libexe.FullName)`" /def:`"$(Resolve-Path (Join-Path $outdir 'obs.def'))`" /out:`"$(Resolve-Path $outdir)\obs.lib`" /machine:x64"
& cmd.exe /c $cmd2 | Out-Null
Get-Item (Join-Path $outdir 'obs.lib') | Select-Object Name, Length
