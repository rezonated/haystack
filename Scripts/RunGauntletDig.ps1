# Copyright (c) 2026 Vanan Andreas.
#
# Runs the Gauntlet multiplayer dig test: a listen server and bot clients dig until someone finds the needle.
# Uses the editor binary in -game mode, so no packaged build is needed.
#
#   .\Scripts\RunGauntletDig.ps1                                  # 2 players dig at a needle 0 to 10 cm deep, 600 s timeout
#   .\Scripts\RunGauntletDig.ps1 -Players 3 -NeedleDepth "20,60"  # deeper needle, three players
#   .\Scripts\RunGauntletDig.ps1 -Random -Timeout 1800           # random spots, no knowledge of the needle
#   .\Scripts\RunGauntletDig.ps1 -Build "C:\Staged\Windows" -Configuration Shipping

param(
    [int]$Players = 2,
    [switch]$Random,
    [string]$NeedleDepth = "0,10",
    [int]$Timeout = 600,
    [string]$Build = "editor",
    [string]$Configuration = "Development"
)

$projectDir = Split-Path -Parent $PSScriptRoot
$proj = Get-ChildItem "$projectDir\*.uproject" | Select-Object -First 1 -ExpandProperty FullName
$assoc = (Get-Content $proj | ConvertFrom-Json).EngineAssociation
$engine = (Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$assoc" -ErrorAction SilentlyContinue).InstalledDirectory
if (-not $engine) { $engine = (Get-ItemProperty "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds" -ErrorAction SilentlyContinue).$assoc }

# -ScriptsForProject makes UAT compile Build/Scripts/Haystack.Automation.csproj, which holds the test nodes.
$args = @(
    "-ScriptsForProject=`"$proj`"",
    "RunUnreal",
    "-project=`"$proj`"",
    "-platform=Win64",
    "-configuration=$Configuration",
    "-build=$Build",
    "-test=HayDig",
    "-HayPlayers=$Players",
    "-HayDigTimeout=$Timeout",
    "-HayNeedleDepth=$NeedleDepth",
    "-log",
    "-logdir=`"$projectDir\Saved\Gauntlet`""
)
if ($Random) { $args += "-HayDigRandom" }

& "$engine\Engine\Build\BatchFiles\RunUAT.bat" @args
exit $LASTEXITCODE
