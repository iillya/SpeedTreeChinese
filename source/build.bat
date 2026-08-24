@echo off
setlocal

rem ---------------------------------------------------------------------------
rem SpeedTree Chinese Localizer - build script
rem Prereq: Visual Studio 2022 C++ build tools, and the Qt 6.5.3 SDK linked at
rem         plugin\qt (an ASCII junction to the Substance plug-in SDK).
rem Output: build\* (hook DLL, launcher, installer)
rem ---------------------------------------------------------------------------

set "ROOT=%~dp0.."
set "CPP=%ROOT%\source"
set "NATIVE=%ROOT%\build"
set "RES=%ROOT%\icon"
set "QT=%ROOT%\qt"
if not exist "%NATIVE%" mkdir "%NATIVE%"
set "OBJDIR=%NATIVE%\obj"
if not exist "%OBJDIR%" mkdir "%OBJDIR%"

if not exist "%QT%\lib\Qt6Widgets.lib" (
    echo [ERROR] A complete Qt SDK was not found at %QT%.
    echo         Create a directory junction named plugin\qt that points to the
    echo         Substance plug-in Qt 6.5.3 SDK ^(headers and .lib files required^).
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
   /Fe:"%NATIVE%\SpeedTreeChineseHook.dll" ^
   /link /IMPLIB:"%OBJDIR%\SpeedTreeChineseHook.lib" ^
   "%QT%\lib\Qt6Core.lib" "%QT%\lib\Qt6Gui.lib" "%QT%\lib\Qt6Widgets.lib" ^
   user32.lib shell32.lib version.lib
if errorlevel 1 exit /b 1

echo === Compiling Qt5 hook DLL ===
set "QT5_SDK=%ROOT%\qt5-sdk"
if not exist "%QT5_SDK%\lib\Qt5Widgets.lib" set "QT5_SDK=%ROOT%\qt5"
if not exist "%QT5_SDK%\lib\Qt5Widgets.lib" (
    echo [WARN] Qt5 SDK not found at %ROOT%\qt5-sdk or %ROOT%\qt5; skipping SpeedTree 9.x Qt5 hook.
) else (
    cl /nologo /O2 /W3 /LD /EHsc /std:c++17 /Zc:__cplusplus /permissive- /utf-8 ^
       /I"%QT5_SDK%\include" ^
       "%CPP%\hook.cpp" ^
       /Fo"%OBJDIR%\hook_qt5.obj" ^
       /Fe:"%NATIVE%\SpeedTreeChineseHook-qt5.dll" ^
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
   /Fe:"%NATIVE%\SpeedTreeChineseLauncher.exe" ^
   /link /SUBSYSTEM:WINDOWS user32.lib
if errorlevel 1 exit /b 1

echo === Compiling installer (elevated) ===
cl /nologo /O2 /W3 /EHsc /std:c++17 /utf-8 ^
   "%CPP%\installer.cpp" ^
   /Fo"%OBJDIR%\installer.obj" ^
   "%OBJDIR%\app_icon.res" ^
   /Fe:"%NATIVE%\SpeedTreeChineseInstaller.exe" ^
   /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib ole32.lib gdi32.lib advapi32.lib ^
   /MANIFEST:EMBED /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
if errorlevel 1 exit /b 1

echo.
echo === Copying runtime dictionary ===
if not exist "%NATIVE%\translations" mkdir "%NATIVE%\translations"
copy /Y "%ROOT%\translations\dictionary_zh.json" "%NATIVE%\translations\dictionary_zh.json" >nul

echo Build complete:
echo   %NATIVE%\SpeedTreeChineseHook.dll
echo   %NATIVE%\SpeedTreeChineseLauncher.exe
echo   %NATIVE%\SpeedTreeChineseInstaller.exe  (payload embedded by embed_files.py)
endlocal
