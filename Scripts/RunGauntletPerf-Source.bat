@echo off
rem Copyright (c) 2026 Vanan Andreas.
rem Gauntlet perf matrix on the source engine registered as VA-UE582-src, the one that can run Shipping.
rem Arguments pass through, for example: RunGauntletPerf-Source.bat -Configurations Development,Shipping
where pwsh >nul 2>nul && (set PS=pwsh) || (set PS=powershell)
%PS% -NoProfile -ExecutionPolicy Bypass -File "%~dp0RunGauntletPerf.ps1" -Engine VA-UE582-src %*
