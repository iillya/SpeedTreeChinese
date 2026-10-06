@echo off
setlocal
set "ROOT=%~dp0.."
set "QT=%~1"
set "MAJOR=%~2"
if not defined QT exit /b 2
if not defined MAJOR exit /b 2
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1
set "OUT=%ROOT%\build\hotkey-test-qt%MAJOR%"
if not exist "%OUT%" mkdir "%OUT%"
cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /O2 /W4 /WX /utf-8 /MD /DQT_NO_DEBUG /DQT_WIDGETS_LIB /DQT_GUI_LIB /DQT_CORE_LIB ^
 /external:I"%QT%\include" /external:W0 "%ROOT%\source\analysis\hotkey_settings_test.cpp" ^
 /Fo"%OUT%\test.obj" /Fe"%OUT%\test.exe" ^
 /link /LIBPATH:"%QT%\lib" Qt%MAJOR%Core.lib Qt%MAJOR%Gui.lib Qt%MAJOR%Widgets.lib Qt%MAJOR%Test.lib
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1
set "PATH=%QT%\bin;%PATH%"
set "QT_PLUGIN_PATH=%QT%\plugins"
set "QT_QPA_PLATFORM=offscreen"
"%OUT%\test.exe"
exit /b %errorlevel%
