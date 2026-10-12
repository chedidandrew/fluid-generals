@echo off
powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0Setup.ps1" %*
if errorlevel 1 echo Setup failed. Read the error above.
pause
