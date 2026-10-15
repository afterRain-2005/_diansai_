@echo off
setlocal
set "INVERTER_ROOT=%~dp0.."
where cl >nul 2>nul
if errorlevel 1 (
  if exist "D:\VisualStudio\VC\Auxiliary\Build\vcvars64.bat" (
    call "D:\VisualStudio\VC\Auxiliary\Build\vcvars64.bat" >nul
  ) else (
    echo Run from an x64 Native Tools Command Prompt for Visual Studio.
    exit /b 1
  )
)
set "INVERTER_TEST_DIR=%~d0\tmp\g474_inverter_checks"
if not exist "%INVERTER_TEST_DIR%" mkdir "%INVERTER_TEST_DIR%"
set "TEMP=%INVERTER_TEST_DIR%"
set "TMP=%INVERTER_TEST_DIR%"
pushd "%INVERTER_TEST_DIR%"
cl /nologo /utf-8 /std:c11 /W3 /WX /I"%INVERTER_ROOT%\Core\Inc" "%INVERTER_ROOT%\tests\inverter_check.c" /Fe:inverter_check.exe
if errorlevel 1 goto failed
inverter_check.exe
if errorlevel 1 goto failed
cl /nologo /utf-8 /std:c11 /W3 /WX /I"%INVERTER_ROOT%\tests\keyboard_mock" /I"%INVERTER_ROOT%\Core\Inc" "%INVERTER_ROOT%\Core\Src\Keyboard.c" "%INVERTER_ROOT%\tests\keyboard_scan_check.c" /Fe:keyboard_scan_check.exe
if errorlevel 1 goto failed
keyboard_scan_check.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
