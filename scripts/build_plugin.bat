@echo off
setlocal EnableExtensions
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSINSTALL=%%i"
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "SDK=third_party\obs-sdk\obs-studio-33.0.0\libobs"
set "FRONTEND=third_party\obs-sdk\obs-studio-33.0.0\frontend\api"
set "TJ=third_party\turbojpeg\libjpeg-turbo-3.2.0\src"
if not exist build\plugin mkdir build\plugin
cl /nologo /W4 /WX /wd4201 /MD /O2 /I"%SDK%" /I"%FRONTEND%" /Iinclude /Iplugin\include /I"%TJ%" ^
   plugin\src\plugin.c plugin\src\media.c plugin\src\transport.c plugin\src\output.c plugin\src\source.c plugin\src\control.c ^
   core\c\protocol.c core\c\protocol_v2.c /LD /Fe:build\plugin\wormhole.dll ^
   /link third_party\obs-sdk\lib\obs.lib third_party\obs-sdk\lib\obs-frontend-api.lib third_party\turbojpeg\build\Release\turbojpeg-static.lib ws2_32.lib
exit /b %errorlevel%
