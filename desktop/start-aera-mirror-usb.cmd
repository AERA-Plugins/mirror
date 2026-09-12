@echo off
setlocal
set "PORT=8080"
where adb.exe >nul 2>nul
if errorlevel 1 (
  if exist "%~dp0platform-tools\adb.exe" (
    set "ADB=%~dp0platform-tools\adb.exe"
  ) else (
    echo ADB was not found. Install Android platform-tools, or extract its
    echo platform-tools folder beside this launcher, then try again.
    pause
    exit /b 1
  )
) else (
  set "ADB=adb.exe"
)

echo Waiting for AERA Recovery over USB...
"%ADB%" wait-for-recovery
if errorlevel 1 goto :failed
"%ADB%" forward --remove tcp:%PORT% >nul 2>nul
"%ADB%" forward tcp:%PORT% tcp:80 >nul
if errorlevel 1 goto :failed

start "" "http://127.0.0.1:%PORT%/"
echo AERA Mirror is active. Press any key to disconnect.
pause >nul
"%ADB%" forward --remove tcp:%PORT% >nul 2>nul
exit /b 0

:failed
echo Could not connect. Start USB Mirror in AERA and keep the USB cable attached.
pause
exit /b 1
