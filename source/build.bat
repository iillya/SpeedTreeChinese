@echo off
setlocal
rem INNO_PRIMARY_BUILD

rem ---------------------------------------------------------------------------
rem SpeedTree Chinese Localizer - build script
rem Prereq: Visual Studio 2022 C++ build tools, and the Qt SDKs (by default the
rem         workspace-shared ..\_ThirdParty\Qt versioned SDK directories,
rem         falling back to third_party\qt6sdk / third_party\qt5sdk).
rem Output:
rem   build\out\*   intermediate binaries (hook DLLs, launcher,
rem                 runtime dictionary) - not intended for direct distribution
rem   dist\         SpeedTreeChineseInstaller.exe (Inno Setup package)
rem ---------------------------------------------------------------------------

set "ROOT=%~dp0.."
set "CPP=%ROOT%\source"
set "OUT=%ROOT%\build\out"
set "OBJDIR=%ROOT%\build\obj"
set "RES=%ROOT%\icon"
set "QT=%ROOT%\..\_ThirdParty\Qt\6.6.0\msvc2019_64"
if not exist "%QT%\lib\Qt6Widgets.lib" set "QT=%ROOT%\third_party\qt6sdk"
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OBJDIR%" mkdir "%OBJDIR%"

if not exist "%QT%\lib\Qt6Widgets.lib" (
    echo [ERROR] A complete Qt SDK was not found at %QT%.
    echo         Expected %ROOT%\..\_ThirdParty\Qt\6.6.0\msvc2019_64 or %ROOT%\third_party\qt6sdk.
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
   /Fo"%OBJDIR%\hook.obj" ^
   /Fe:"%OUT%\SpeedTreeChineseHook.dll" ^
   /link /IMPLIB:"%OBJDIR%\SpeedTreeChineseHook.lib" ^
   "%QT%\lib\Qt6Core.lib" "%QT%\lib\Qt6Gui.lib" "%QT%\lib\Qt6Widgets.lib" ^
   user32.lib shell32.lib version.lib
if errorlevel 1 exit /b 1

echo === Compiling Qt5 hook DLL ===
set "QT5_SDK=%ROOT%\..\_ThirdParty\Qt\5.12.5\msvc2017_64"
if not exist "%QT5_SDK%\lib\Qt5Widgets.lib" set "QT5_SDK=%ROOT%\third_party\qt5sdk"
if not exist "%QT5_SDK%\lib\Qt5Widgets.lib" (
    echo [WARN] Qt5 SDK not found at %QT5_SDK%; skipping SpeedTree 9.x Qt5 hook.
) else (
    cl /nologo /O2 /W3 /LD /EHsc /std:c++17 /Zc:__cplusplus /permissive- /utf-8 ^
       /I"%QT5_SDK%\include" ^
       "%CPP%\hook.cpp" ^
       /Fo"%OBJDIR%\hook_qt5.obj" ^
       /Fe:"%OUT%\SpeedTreeChineseHook-qt5.dll" ^
       /link /IMPLIB:"%OBJDIR%\SpeedTreeChineseHook-qt5.lib" ^
       "%QT5_SDK%\lib\Qt5Core.lib" "%QT5_SDK%\lib\Qt5Gui.lib" "%QT5_SDK%\lib\Qt5Widgets.lib" ^
       user32.lib shell32.lib version.lib
)

echo === Compiling launcher ===
rc /nologo /fo "%OBJDIR%\app_icon.res" "%RES%\app_icon.rc"
if errorlevel 1 exit /b 1
cl /nologo /O2 /W3 /EHsc /utf-8 ^
   "%CPP%\launcher.cpp" ^
   /Fo"%OBJDIR%\launcher.obj" ^
   "%OBJDIR%\app_icon.res" ^
   /Fe:"%OUT%\SpeedTreeChineseLauncher.exe" ^
   /link /SUBSYSTEM:WINDOWS user32.lib version.lib
if errorlevel 1 exit /b 1

echo === Staging runtime dictionary (next to the hook for dev launches) ===
if not exist "%OUT%\translations" mkdir "%OUT%\translations"
copy /Y "%ROOT%\translations\dictionary_zh.json" "%OUT%\translations\dictionary_zh.json" >nul

echo === Packing dist installer ===
if /I "%~1"=="--runtime-only" exit /b 0
call "%~dp0build_inno.bat" --package-only
if errorlevel 1 exit /b 1

echo Build complete:
echo   %OUT%\SpeedTreeChineseHook.dll
echo   %OUT%\SpeedTreeChineseLauncher.exe
echo   dist\SpeedTreeChineseInstaller.exe  (single distributable)
endlocal
