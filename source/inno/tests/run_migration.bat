@echo off
setlocal
set "ROOT=%~dp0..\..\.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist "%ROOT%\build\inno" mkdir "%ROOT%\build\inno"
cl /nologo /O2 /W4 /WX /MT /EHsc /std:c++17 /utf-8 /external:env:INCLUDE /external:W0 "%~dp0legacy_migration_test.cpp" /Fo:"%ROOT%\build\inno\legacy-migration-test.obj" /Fe:"%ROOT%\build\inno\legacy-migration-test.exe" /link version.lib advapi32.lib shell32.lib shlwapi.lib
if not "%errorlevel%"=="0" exit /b %errorlevel%
"%ROOT%\build\inno\legacy-migration-test.exe"
exit /b %errorlevel%
