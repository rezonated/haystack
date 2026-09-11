@echo off
rem Copyright (c) 2026 Vanan Andreas.
rem Hosts a listen server other machines can join with Join.bat <this machine's IP>. Extra arguments pass through, for example -windowed.
start "Haystack host" "%~dp0Haystack.exe" /Game/ThirdPersonBP/Maps/ThirdPersonExampleMap?listen %*
