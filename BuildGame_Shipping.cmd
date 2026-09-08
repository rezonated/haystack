@echo off
PowerShell.exe -NoProfile -NonInteractive -NoLogo -ExecutionPolicy Bypass -Command "& '%~dp0/Tools/Scripts/BuildGame.ps1' -Config Shipping"
if %ERRORLEVEL% neq 0 (pause)
