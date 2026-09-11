@echo off
rem Copyright (c) 2026 Vanan Andreas.
rem Joins a listen server: Join.bat 192.168.1.20. Without an address it joins a server on this machine.
set ADDRESS=%~1
if "%ADDRESS%"=="" set ADDRESS=127.0.0.1
start "Haystack client" "%~dp0Haystack.exe" %ADDRESS%
