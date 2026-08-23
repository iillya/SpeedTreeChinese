@echo off
setlocal

rem ---------------------------------------------------------------------------
rem SpeedTree Chinese Localizer - build script
rem Prereq: Visual Studio 2022 C++ build tools, and the Qt 6.5.3 SDK linked at
rem         plugin\qt (an ASCII junction to the Substance plug-in SDK).
rem Output: plugin\native\* (hook DLL, launcher, installer)
rem ---------------------------------------------------------------------------

set "ROOT=%~dp0.."
set "CPP=%ROOT%\cpp"
set "NATIVE=%ROOT%\native"
set "RES=%ROOT%\resources"
set "QT=%ROOT%\qt"
if not exist "%NATIVE%" mkdir "%NATIVE%"

if not exist "%QT%\lib\Qt6Widgets.lib" (
    echo [ERROR] Qt SDK not found at %QT%. Create an ASCII junction named plugin\qt
    echo         pointing to the Substance plug-in Qt 6.5.3 SDK.
    exit /b 1
)

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo [ERROR] vcvars64.bat not found. Install Visual Studio 2022 Build Tools.
    exit /b 1
)
call "%VCVARS%" >nul

echo === Compiling hook DLL ===
cl /nologo /O2 /W3 /LD /EHsc /std:c++17 /Zc:__cplusplus /permissive- /utf-8 ^
   /I"%QT%\include" ^
   "%CPP%\hook.cpp" ^
   /Fe:"%NATIVE%\SpeedTreeChineseHook.dll" ^
   /link /IMPLIB:"%NATIVE%\SpeedTreeChineseHook.lib" ^
   "%QT%\lib\Qt6Core.lib" "%QT%\lib\Qt6Gui.lib" "%QT%\lib\Qt6Widgets.lib" ^
   user32.lib shell32.lib version.lib
if errorlevel 1 exit /b 1

echo === Compiling launcher ===
rc /nologo /fo "%NATIVE%\app_icon.res" "%RES%\app_icon.rc"
if errorlevel 1 exit /b 1
cl /nologo /O2 /W3 /EHsc /utf-8 ^
   "%CPP%\launcher.cpp" ^
   "%NATIVE%\app_icon.res" ^
   /Fe:"%NATIVE%\SpeedTreeChineseLauncher.exe" ^
   /link /SUBSYSTEM:WINDOWS user32.lib
if errorlevel 1 exit /b 1

echo === Compiling installer (elevated) ===
cl /nologo /O2 /W3 /EHsc /utf-8 ^
   "%CPP%\installer.cpp" "%NATIVE%\app_icon.res" ^
   /Fe:"%NATIVE%\SpeedTreeChineseInstaller.exe" ^
   /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib ole32.lib gdi32.lib advapi32.lib ^
   /MANIFEST:EMBED /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
if errorlevel 1 exit /b 1

echo.
echo Build complete:
echo   %NATIVE%\SpeedTreeChineseHook.dll
echo   %NATIVE%\SpeedTreeChineseLauncher.exe
echo   %NATIVE%\SpeedTreeChineseInstaller.exe  (payload embedded by embed_files.py)
endlocal
