@echo off
setlocal

set "ROOT=%~dp0.."
set "OUT=%ROOT%\probe\out"
if not exist "%OUT%" mkdir "%OUT%"

rem ---- Qt 6.5.3 SDK bundled with the Substance plug-in (ABI-compatible) ----
set "QT_ROOT=%~dp0..\..\sp插件\substance3d_chinese_translator\sdks\qt\6.5.3\msvc2019_64"
if not exist "%QT_ROOT%\lib\Qt6Widgets.lib" (
    echo [ERROR] Qt SDK not found: %QT_ROOT%
    exit /b 1
)

rem ---- MSVC environment ----
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo [ERROR] vcvars64.bat not found.
    exit /b 1
)
call "%VCVARS%" >nul

rem ---- Probe hook DLL ----
cl /nologo /O2 /W3 /LD /EHsc /std:c++17 /utf-8 ^
   /I"%QT_ROOT%\include" ^
   "%~dp0src\dllmain.cpp" ^
   /Fe:"%OUT%\SpeedTreeChineseHook.dll" ^
   /link /IMPLIB:"%OUT%\SpeedTreeChineseHook.lib" ^
   "%QT_ROOT%\lib\Qt6Core.lib" ^
   "%QT_ROOT%\lib\Qt6Gui.lib" ^
   "%QT_ROOT%\lib\Qt6Widgets.lib" ^
   user32.lib shell32.lib version.lib
if errorlevel 1 exit /b 1

rem ---- Probe launcher EXE ----
cl /nologo /O2 /W3 /EHsc /utf-8 ^
   "%~dp0src\launcher.cpp" ^
   /Fe:"%OUT%\SpeedTreeChineseLauncher.exe" ^
   /link /SUBSYSTEM:WINDOWS user32.lib
if errorlevel 1 exit /b 1

echo.
echo Build complete.
echo   DLL : %OUT%\SpeedTreeChineseHook.dll
echo   EXE : %OUT%\SpeedTreeChineseLauncher.exe
endlocal
