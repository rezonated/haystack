@echo off
rem Copyright (c) 2026 Vanan Andreas.
rem Gauntlet multiplayer dig test on the source engine registered as VA-UE582-src.
rem Arguments pass through, for example: RunGauntletDig-Source.bat -Players 3 -Random
where pwsh >nul 2>nul && (set PS=pwsh) || (set PS=powershell)
%PS% -NoProfile -ExecutionPolicy Bypass -File "%~dp0RunGauntletDig.ps1" -Engine VA-UE582-src %*
