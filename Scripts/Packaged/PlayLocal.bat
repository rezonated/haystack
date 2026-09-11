@echo off
rem Copyright (c) 2026 Vanan Andreas.
rem Starts a listen server and a client of this build side by side on this machine. Add -log to a line for a log window.
set MAP=/Game/ThirdPersonBP/Maps/ThirdPersonExampleMap
set WINDOW=-windowed -ResX=960 -ResY=540
start "Haystack host" "%~dp0Haystack.exe" %MAP%?listen %WINDOW% -WinX=0 -WinY=40
timeout /t 5 /nobreak >nul
start "Haystack client" "%~dp0Haystack.exe" 127.0.0.1 %WINDOW% -WinX=980 -WinY=40
