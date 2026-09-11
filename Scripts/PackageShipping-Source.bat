@echo off
rem Copyright (c) 2026 Vanan Andreas.
rem Packages the Shipping game with the source engine registered as VA-UE582-src, then zips it.
rem Arguments pass through, for example: PackageShipping-Source.bat -NoZip
where pwsh >nul 2>nul && (set PS=pwsh) || (set PS=powershell)
%PS% -NoProfile -ExecutionPolicy Bypass -File "%~dp0PackageShipping.ps1" -Engine VA-UE582-src %*
