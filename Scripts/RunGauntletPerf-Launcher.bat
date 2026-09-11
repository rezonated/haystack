@echo off
rem Copyright (c) 2026 Vanan Andreas.
rem Gauntlet perf matrix on the launcher engine 5.8. Development only, an installed engine has no log or CSV in Shipping.
rem Arguments pass through, for example: RunGauntletPerf-Launcher.bat -Resolutions 1920x1080 -Capture 30
where pwsh >nul 2>nul && (set PS=pwsh) || (set PS=powershell)
%PS% -NoProfile -ExecutionPolicy Bypass -File "%~dp0RunGauntletPerf.ps1" -Engine 5.8 %*
