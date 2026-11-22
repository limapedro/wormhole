$ErrorActionPreference='Stop'
$vs = 'C:\Program Files\Microsoft Visual Studio\18\Community'
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$dumpbin = Get-ChildItem (Join-Path $vs 'VC\Tools\MSVC') -Recurse -Filter dumpbin.exe |
    Where-Object { $_.FullName -match 'bin\\Hostx64\\x64' } | Select-Object -First 1
$libexe = Get-ChildItem (Join-Path $vs 'VC\Tools\MSVC') -Recurse -Filter lib.exe |
    Where-Object { $_.FullName -match 'bin\\Hostx64\\x64' } | Select-Object -First 1
$dll = 'C:\Program Files\obs-studio\bin\64bit\obs-frontend-api.dll'
$outdir = 'third_party/obs-sdk/lib'
$cmd = "`"$vcvars`" && `"$($dumpbin.FullName)`" /exports `"$dll`""
$exports = & cmd.exe /c $cmd
$names = $exports | ForEach-Object {
    if ($_ -match '^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)') { $Matches[1] }
} | Sort-Object -Unique
$def = @('LIBRARY obs-frontend-api', 'EXPORTS') + ($names | ForEach-Object { "    $_" })
[IO.File]::WriteAllLines((Join-Path $outdir 'obs-frontend-api.def'), $def)
$cmd2 = "`"$vcvars`" && `"$($libexe.FullName)`" /def:`"$(Resolve-Path (Join-Path $outdir 'obs-frontend-api.def'))`" /out:`"$(Resolve-Path $outdir)\obs-frontend-api.lib`" /machine:x64"
& cmd.exe /c $cmd2 | Out-Null
"exports=$($names.Count)"
Get-Item (Join-Path $outdir 'obs-frontend-api.lib') | Select-Object Name,Length
