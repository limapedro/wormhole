@echo off
setlocal EnableExtensions
rem Build and run the all-C Wormhole protocol tests with Visual Studio / MSVC.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo error: vswhere.exe not found
    exit /b 1
)
set "VSINSTALL="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSINSTALL=%%i"
if "%VSINSTALL%"=="" (
    echo error: Visual Studio installation not found
    exit /b 1
)
echo Using Visual Studio: %VSINSTALL%
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\c mkdir build\c
cl /nologo /W4 /WX /Od /Zi /Iinclude core\c\protocol.c tests\test_protocol.c /Fe:build\c\test_protocol.exe
if errorlevel 1 exit /b 1
cl /nologo /W4 /WX /Od /Zi /Iinclude core\c\protocol.c tests\capture_writer.c /Fe:build\c\capture_writer.exe
if errorlevel 1 exit /b 1
cl /nologo /W4 /WX /Od /Zi /Iinclude /Ivendor core\c\protocol.c core\c\harness.c core\c\proxy.c core\c\quality.c core\c\differential.c core\c\harness_main.c /Fe:build\c\wormhole-harness.exe /link ws2_32.lib
if errorlevel 1 exit /b 1
build\c\test_protocol.exe
exit /b %errorlevel%
