# Copyright (c) 2026 Vanan Andreas.
#
# Runs the Haystack automation tests headless and prints one line per test.
#
#   .\Scripts\RunTests.ps1                    # everything under Haystack.
#   .\Scripts\RunTests.ps1 -Filter Haystack.Layout

param(
    [string]$Filter = "Haystack"
)

$projectDir = Split-Path -Parent $PSScriptRoot
$proj = Get-ChildItem "$projectDir\*.uproject" | Select-Object -First 1 -ExpandProperty FullName
$assoc = (Get-Content $proj | ConvertFrom-Json).EngineAssociation
$engine = (Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$assoc" -ErrorAction SilentlyContinue).InstalledDirectory
if (-not $engine) { $engine = (Get-ItemProperty "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds" -ErrorAction SilentlyContinue).$assoc }
$exe = "$engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$log = "$projectDir\Saved\Logs\Tests.log"

& $exe $proj -ExecCmds="Automation RunTests $Filter;Quit" -unattended -nopause -nosplash -nullrhi -log LOG=Tests.log 2>&1 | Out-Null
$exit = $LASTEXITCODE

Select-String -Path $log -Pattern "Test Completed|LogAutomationController: Error|Fatal error" | Select-Object -ExpandProperty Line | ForEach-Object { $_ -replace '^\[[^\]]+\]\[\s*\d+\]LogAutomationController: Display: ', '' }
"process exit code $exit"
exit $exit
