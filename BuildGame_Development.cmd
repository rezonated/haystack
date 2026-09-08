@echo off
PowerShell.exe -NoProfile -NonInteractive -NoLogo -ExecutionPolicy Bypass -Command "& '%~dp0/Tools/Scripts/BuildGame.ps1' -Config Development"
if %ERRORLEVEL% neq 0 (pause)
