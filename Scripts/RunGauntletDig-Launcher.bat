@echo off
rem Copyright (c) 2026 Vanan Andreas.
rem Gauntlet multiplayer dig test on the launcher engine 5.8.
rem Arguments pass through, for example: RunGauntletDig-Launcher.bat -Players 3 -Random
where pwsh >nul 2>nul && (set PS=pwsh) || (set PS=powershell)
%PS% -NoProfile -ExecutionPolicy Bypass -File "%~dp0RunGauntletDig.ps1" -Engine 5.8 %*
