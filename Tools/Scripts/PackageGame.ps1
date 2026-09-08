# Copyright (c) 2026 Vanan Andreas.

param(
    [string]$Config = "Development"
)

. $PSScriptRoot/SetupEnv.ps1

$archiveDir = Join-Path $projectRoot "Packaged"

Write-Host "============================================="
Write-Host " PACKAGE GAME - $Config"
Write-Host " Project : $projectName"
Write-Host " Archive : $archiveDir"
Write-Host "============================================="

& $uat BuildCookRun `
    -project="$uprojectPath" `
    -platform=Win64 `
    "-clientconfig=$Config" `
    -build `
    -cook `
    -stage `
    -package `
    -pak `
    -iostore `
    -nop4 `
    -archive `
    -archivedirectory="$archiveDir" `
    -utf8output
