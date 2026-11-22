@echo off
setlocal EnableExtensions
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSINSTALL=%%i"
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "SDK=third_party\obs-sdk\obs-studio-33.0.0\libobs"
if not exist build\plugin mkdir build\plugin
cl /nologo /W4 /WX /wd4201 /O2 /I"%SDK%" /Iinclude plugin\src\plugin.c /LD /Fe:build\plugin\wormhole.dll /link third_party\obs-sdk\lib\obs.lib
exit /b %errorlevel%
